#include "controllers/SessionController.hpp"

#include <crow.h>

#include <exception>

#include "dto/PlanDto.hpp"  // reuse plan_response for GET /my/plan
#include "dto/SessionDto.hpp"
#include "http/ApiError.hpp"
#include "http/AuthGuard.hpp"
#include "middleware/JwtAuthMiddleware.hpp"
#include "util/Jwt.hpp"

namespace fitplan::controllers {

void register_session_routes(app::FitPlanApp& app, services::SessionService& sessions,
                             repositories::CoachTraineeRepository& roster) {
    // GET /api/my/plan -----------------------------------------------------
    CROW_ROUTE(app, "/api/my/plan")
    ([&app, &sessions](const crow::request& req) {
        try {
            const auto& ctx = app.template get_context<middleware::JwtAuthMiddleware>(req);
            const util::TokenClaims claims = http::require_role(ctx, "trainee");
            return dto::plan_response(200, sessions.active_plan_for(claims.user_id));
        } catch (const std::exception& ex) {
            return http::problem_response_for(ex);
        }
    });

    // GET /api/my/sessions -----------------------------------------------------
    CROW_ROUTE(app, "/api/my/sessions")
    ([&app, &sessions](const crow::request& req) {
        try {
            const auto& ctx = app.template get_context<middleware::JwtAuthMiddleware>(req);
            const util::TokenClaims claims = http::require_role(ctx, "trainee");
            return dto::session_list_response(sessions.list_sessions(claims.user_id));
        } catch (const std::exception& ex) {
            return http::problem_response_for(ex);
        }
    });

    // POST /api/my/sessions -------------------------------------------------
    CROW_ROUTE(app, "/api/my/sessions")
        .methods(crow::HTTPMethod::Post)([&app, &sessions](const crow::request& req) {
            try {
                const auto& ctx = app.template get_context<middleware::JwtAuthMiddleware>(req);
                const util::TokenClaims claims = http::require_role(ctx, "trainee");

                const services::SessionInput input = dto::parse_session_request(req.body);
                return dto::session_response(201, sessions.log_session(claims.user_id, input));
            } catch (const std::exception& ex) {
                return http::problem_response_for(ex);
            }
        });

    // PATCH /api/my/sessions/<int> ---------------------------------------
    CROW_ROUTE(app, "/api/my/sessions/<int>")
        .methods(crow::HTTPMethod::Patch)([&app, &sessions](const crow::request& req, int id) {
            try {
                const auto& ctx = app.template get_context<middleware::JwtAuthMiddleware>(req);
                const util::TokenClaims claims = http::require_role(ctx, "trainee");

                const services::SessionPatch patch = dto::parse_session_patch(req.body);
                return dto::session_response(200,
                                             sessions.update_session(claims.user_id, id, patch));
            } catch (const std::exception& ex) {
                return http::problem_response_for(ex);
            }
        });

    // DELETE /api/my/sessions/<int> ------------------------------------
    CROW_ROUTE(app, "/api/my/sessions/<int>")
        .methods(crow::HTTPMethod::Delete)([&app, &sessions](const crow::request& req, int id) {
            try {
                const auto& ctx = app.template get_context<middleware::JwtAuthMiddleware>(req);
                const util::TokenClaims claims = http::require_role(ctx, "trainee");
                sessions.delete_session(claims.user_id, id);
                return crow::response(204);
            } catch (const std::exception& ex) {
                return http::problem_response_for(ex);
            }
        });

    // GET /api/trainees/<int>/sessions ---------------------------------------
    // A coach's read-only window onto one trainee's log. Same roster gate as
    // GET /api/trainees/<int>/progress: a coach may only read a trainee who is
    // on their roster; to anyone else that trainee looks like a 404.
    CROW_ROUTE(app, "/api/trainees/<int>/sessions")
    ([&app, &sessions, &roster](const crow::request& req, int trainee_id) {
        try {
            const auto& ctx = app.template get_context<middleware::JwtAuthMiddleware>(req);
            const util::TokenClaims claims = http::require_role(ctx, "coach");

            if (!roster.is_linked(claims.user_id, trainee_id)) {
                throw http::ApiError(http::ApiErrorKind::kNotFound,
                                     "that trainee is not on your roster");
            }
            return dto::session_list_response(sessions.list_sessions(trainee_id));
        } catch (const std::exception& ex) {
            return http::problem_response_for(ex);
        }
    });
}

}  // namespace fitplan::controllers