#include "controllers/ProgressController.hpp"

#include <crow.h>

#include <chrono>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "dto/ProgressDto.hpp"
#include "http/ApiError.hpp"
#include "http/AuthGuard.hpp"
#include "middleware/JwtAuthMiddleware.hpp"
#include "services/ProgressService.hpp"
#include "util/Jwt.hpp"

namespace fitplan::controllers {

namespace {

// Today as "YYYY-MM-DD", UTC. Passed to ProgressService::weekly_streak so that
// function stays pure (the clock is injected, not read inside it).
std::string today_iso() {
    const auto today = std::chrono::floor<std::chrono::days>(
        std::chrono::system_clock::now());
    const std::chrono::year_month_day ymd{today};
    std::ostringstream out;
    out << static_cast<int>(ymd.year()) << '-' << std::setfill('0')
        << std::setw(2) << static_cast<unsigned>(ymd.month()) << '-'
        << std::setw(2) << static_cast<unsigned>(ymd.day());
    return out.str();
}

// Run every ProgressService calculation over one trainee's data.
dto::ProgressReport build_report(services::SessionService& sessions,
                                 std::int64_t trainee_id) {
    using services::ProgressService;

    const std::vector<services::LoggedSet> logged =
        sessions.logged_sets_for(trainee_id);
    const std::vector<services::PrescribedItem> prescribed =
        sessions.prescribed_for(trainee_id);

    dto::ProgressReport report;
    report.trainee_id = trainee_id;
    report.as_of = today_iso();
    report.total_volume = ProgressService::total_volume(logged);
    report.adherence = ProgressService::adherence(logged, prescribed);
    report.weekly_streak = ProgressService::weekly_streak(logged, report.as_of);
    report.volume_over_time = ProgressService::volume_over_time(logged);

    std::set<std::int64_t> exercise_ids;
    for (const services::LoggedSet& s : logged) {
        exercise_ids.insert(s.exercise_id);
    }
    for (const std::int64_t ex_id : exercise_ids) {
        dto::ExerciseE1rmSeries e;
        e.exercise_id = ex_id;
        e.series = ProgressService::best_e1rm_over_time(logged, ex_id);
        if (!e.series.empty()) {
            report.exercises.push_back(std::move(e));
        }
    }
    return report;
}

}  // namespace

void register_progress_routes(app::FitPlanApp& app,
                              services::SessionService& sessions,
                              repositories::CoachTraineeRepository& roster) {
    // GET /api/my/progress -----------------------------------------------------
    CROW_ROUTE(app, "/api/my/progress")
        ([&app, &sessions](const crow::request& req) {
            try {
                const auto& ctx =
                    app.template get_context<middleware::JwtAuthMiddleware>(req);
                const util::TokenClaims claims =
                    http::require_role(ctx, "trainee");
                return dto::progress_response(
                    build_report(sessions, claims.user_id));
            } catch (const std::exception& ex) {
                return http::problem_response_for(ex);
            }
        });

    // GET /api/trainees/<int>/progress -----------------------------------
    CROW_ROUTE(app, "/api/trainees/<int>/progress")
        ([&app, &sessions, &roster](const crow::request& req, int trainee_id) {
            try {
                const auto& ctx =
                    app.template get_context<middleware::JwtAuthMiddleware>(req);
                const util::TokenClaims claims =
                    http::require_role(ctx, "coach");

                if (!roster.is_linked(claims.user_id, trainee_id)) {
                    throw http::ApiError(http::ApiErrorKind::kNotFound,
                                         "that trainee is not on your roster");
                }
                return dto::progress_response(build_report(sessions, trainee_id));
            } catch (const std::exception& ex) {
                return http::problem_response_for(ex);
            }
        });
}

}  // namespace fitplan::controllers
