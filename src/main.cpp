#include <crow.h>
#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <exception>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

#include "app/App.hpp"
#include "config/Config.hpp"
#include "controllers/AuthController.hpp"
#include "controllers/ExerciseController.hpp"
#include "controllers/ExerciseNoteController.hpp"
#include "controllers/PlanController.hpp"
#include "controllers/ProgressController.hpp"
#include "controllers/SessionController.hpp"
#include "controllers/TraineeController.hpp"
#include "db/Database.hpp"
#include "fitplan/Version.hpp"
#include "middleware/JwtAuthMiddleware.hpp"
#include "repositories/CoachTraineeRepository.hpp"
#include "repositories/EmailVerificationTokenRepository.hpp"
#include "repositories/ExerciseNoteRepository.hpp"
#include "repositories/ExerciseRepository.hpp"
#include "repositories/PlanItemRepository.hpp"
#include "repositories/PlanRepository.hpp"
#include "repositories/SessionRepository.hpp"
#include "repositories/SessionSetRepository.hpp"
#include "repositories/UserRepository.hpp"
#include "services/AuthService.hpp"
#include "services/BrevoEmailSender.hpp"
#include "services/EmailSender.hpp"
#include "services/EmailVerificationService.hpp"
#include "services/GoogleIdTokenVerifier.hpp"
#include "services/GoogleJwks.hpp"
#include "services/LogEmailSender.hpp"
#include "services/PlanService.hpp"
#include "services/SessionService.hpp"
#include "util/Clock.hpp"

namespace {

crow::response json_response(int status_code, const nlohmann::json& body) {
    crow::response res(status_code);
    res.body = body.dump();
    res.set_header("Content-Type", "application/json");
    return res;
}

bool has_flag(int argc, char** argv, std::string_view flag) {
    for (int i = 1; i < argc; ++i) {
        if (flag == argv[i]) {
            return true;
        }
    }
    return false;
}

std::string read_text_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

spdlog::level::level_enum to_spdlog_level(const std::string& name) {
    const spdlog::level::level_enum level = spdlog::level::from_str(name);
    // from_str() returns 'off' for unrecognized input; only honor that when the
    // caller genuinely asked for "off".
    if (level == spdlog::level::off && name != "off") {
        spdlog::warn("Config: unknown FITPLAN_LOG_LEVEL='{}'; falling back to 'info'", name);
        return spdlog::level::info;
    }
    return level;
}

void configure_logging(const std::string& level_name) {
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");
    spdlog::set_level(to_spdlog_level(level_name));
    spdlog::flush_on(spdlog::level::warn);
}

}  // namespace

int main(int argc, char** argv) {
    const fitplan::Config config = fitplan::Config::from_env();
    configure_logging(config.log_level);

    spdlog::info("{} {} starting up", fitplan::kProjectName, fitplan::kVersion);
    if (config.uses_insecure_jwt_secret()) {
        spdlog::warn("FITPLAN_JWT_SECRET is unset - using an insecure development secret");
    }

    std::optional<fitplan::db::Database> database;
    try {
        database.emplace(config.database_path, config.migrations_dir);
        spdlog::info("Database '{}' ready at schema version {}", config.database_path,
                     database->schema_version());

        if (has_flag(argc, argv, "--seed")) {
            const std::string seed_sql = read_text_file("scripts/seed.sql");
            if (seed_sql.empty()) {
                spdlog::error("--seed: could not read scripts/seed.sql (run from the repo root)");
                return 1;
            }
            database->connection().exec(seed_sql);
            spdlog::info("--seed: sample data loaded");
        }
    } catch (const std::exception& ex) {
        spdlog::critical("Database initialization failed: {}", ex.what());
        return 1;
    }

    const int schema_version = database->schema_version();

    // Data-access and business layers, built once and shared by every request.
    fitplan::repositories::UserRepository users(database->connection());
    fitplan::repositories::ExerciseRepository exercises(database->connection());
    fitplan::repositories::PlanRepository plans_repo(database->connection());
    fitplan::repositories::PlanItemRepository plan_items(database->connection());
    fitplan::repositories::CoachTraineeRepository roster(database->connection());
    fitplan::repositories::ExerciseNoteRepository exercise_notes(database->connection());
    fitplan::repositories::SessionRepository sessions_repo(database->connection());
    fitplan::repositories::SessionSetRepository session_sets(database->connection());
    fitplan::repositories::EmailVerificationTokenRepository email_tokens(database->connection());

    // Google Sign-In is optional: build the verifier only when a client id is
    // configured. Both objects must outlive `auth`, which borrows the verifier.
    std::optional<fitplan::services::GoogleJwksCache> google_jwks;
    std::optional<fitplan::services::GoogleIdTokenVerifier> google_verifier;
    if (config.google_sign_in_enabled()) {
        google_jwks.emplace();  // default fetcher: a real HTTPS GET of Google's JWKS
        google_verifier.emplace(config.google_client_id, *google_jwks);
        spdlog::info("Google Sign-In enabled (client id {})", config.google_client_id);
    } else {
        spdlog::info("Google Sign-In disabled (FITPLAN_GOOGLE_CLIENT_ID is unset)");
    }

    // Email verification is always on for local sign-ups. With a Brevo key the
    // code is emailed; without one it goes to the log so development still works.
    fitplan::services::EmailSender email_sender =
        config.transactional_email_enabled()
            ? fitplan::services::make_brevo_email_sender(
                  {config.brevo_api_key, config.email_from, config.email_from_name})
            : fitplan::services::make_log_email_sender();
    if (config.transactional_email_enabled()) {
        spdlog::info("Transactional email enabled (Brevo, from {})", config.email_from);
    } else {
        spdlog::warn(
            "FITPLAN_BREVO_API_KEY is unset - verification codes go to the log, not to email");
    }
    fitplan::services::EmailVerificationService email_verification(
        users, email_tokens, email_sender, fitplan::util::iso_utc_now, config.public_base_url);

    fitplan::services::AuthService auth(users, config.jwt_secret, config.jwt_ttl_seconds,
                                        google_verifier ? &*google_verifier : nullptr,
                                        &email_verification);
    fitplan::services::PlanService plan_service(database->connection(), plans_repo, plan_items,
                                                roster, exercises);
    fitplan::services::SessionService session_service(
        database->connection(), sessions_repo, session_sets, plans_repo, plan_items, exercises);

    fitplan::app::FitPlanApp app;
    app.loglevel(crow::LogLevel::Warning);
    app.get_middleware<fitplan::middleware::JwtAuthMiddleware>().secret = config.jwt_secret;

    CROW_ROUTE(app, "/api/health")
    ([schema_version]() {
        const nlohmann::json body{
            {"status", "ok"},
            {"service", fitplan::kProjectName},
            {"version", fitplan::kVersion},
            {"schema_version", schema_version},
        };
        return json_response(200, body);
    });

    fitplan::controllers::register_auth_routes(app, auth, config.google_client_id);
    fitplan::controllers::register_exercise_routes(app, exercises);
    fitplan::controllers::register_plan_routes(app, plan_service);
    fitplan::controllers::register_trainee_routes(app, users, roster);
    fitplan::controllers::register_session_routes(app, session_service, roster);
    fitplan::controllers::register_progress_routes(app, session_service, roster, exercises);
    fitplan::controllers::register_exercise_note_routes(app, exercise_notes, plans_repo,
                                                        plan_items);

    // API documentation: the OpenAPI spec plus a Swagger UI that renders it.
    // These literal routes are matched ahead of the "/<path>" frontend catch-all.
    const std::string docs_dir = config.docs_dir;
    CROW_ROUTE(app, "/openapi.yaml")
    ([docs_dir](const crow::request&, crow::response& res) {
        res.set_static_file_info(docs_dir + "/openapi.yaml");
        res.end();
    });
    CROW_ROUTE(app, "/docs")
    ([docs_dir](const crow::request&, crow::response& res) {
        res.set_static_file_info(docs_dir + "/swagger-ui/index.html");
        res.end();
    });
    CROW_ROUTE(app, "/docs/<path>")
    ([docs_dir](const crow::request&, crow::response& res, const std::string& asset) {
        res.set_static_file_info(docs_dir + "/swagger-ui/" + asset);
        res.end();
    });

    // Static frontend. Every non-API GET resolves to a file under web_dir, with
    // "/" mapping to index.html. crow::response::set_static_file_info() rejects
    // "../" traversal and fills in Content-Type / Content-Length / 404 for us.
    const std::string web_dir = config.web_dir;
    CROW_ROUTE(app, "/")
    ([web_dir](const crow::request&, crow::response& res) {
        res.set_static_file_info(web_dir + "/index.html");
        res.end();
    });
    CROW_ROUTE(app, "/<path>")
    ([web_dir](const crow::request&, crow::response& res, const std::string& asset) {
        res.set_static_file_info(web_dir + "/" + asset);
        res.end();
    });

    app.bindaddr(config.host).port(config.port);
    if (config.thread_count > 0) {
        app.concurrency(static_cast<std::uint16_t>(config.thread_count));
    } else {
        app.multithreaded();
    }

    spdlog::info("Listening on http://{}:{}", config.host, config.port);

    try {
        app.run();
    } catch (const std::exception& ex) {
        spdlog::critical("Server terminated with an exception: {}", ex.what());
        return 1;
    }

    spdlog::info("Shutdown complete");
    return 0;
}
