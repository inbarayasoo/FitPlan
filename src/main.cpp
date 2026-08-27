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

#include "config/Config.hpp"
#include "db/Database.hpp"
#include "fitplan/Version.hpp"

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

    crow::SimpleApp app;
    app.loglevel(crow::LogLevel::Warning);

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
