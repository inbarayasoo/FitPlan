#include "controllers/TraineeController.hpp"

#include <crow.h>

#include <exception>
#include <optional>

#include "dto/AuthDto.hpp"
#include "dto/TraineeDto.hpp"
#include "http/ApiError.hpp"
#include "http/AuthGuard.hpp"
#include "middleware/JwtAuthMiddleware.hpp"
#include "models/User.hpp"
#include "util/Jwt.hpp"

namespace fitplan::controllers {

void register_trainee_routes(app::FitPlanApp& app, repositories::UserRepository& users,
                             repositories::CoachTraineeRepository& roster) {
    // GET /api/trainees ------------------------------------------------------
    CROW_ROUTE(app, "/api/trainees")
    ([&app, &roster](const crow::request& req) {
        try {
            const auto& ctx = app.template get_context<middleware::JwtAuthMiddleware>(req);
            const util::TokenClaims claims = http::require_role(ctx, "coach");
            return dto::trainee_list_response(roster.list_trainees(claims.user_id));
        } catch (const std::exception& ex) {
            return http::problem_response_for(ex);
        }
    });

    // POST /api/trainees ---------------------------------------------------
    CROW_ROUTE(app, "/api/trainees")
        .methods(crow::HTTPMethod::Post)([&app, &users, &roster](const crow::request& req) {
            try {
                const auto& ctx = app.template get_context<middleware::JwtAuthMiddleware>(req);
                const util::TokenClaims claims = http::require_role(ctx, "coach");

                const dto::AttachTraineeRequest body = dto::parse_attach_trainee_request(req.body);

                const std::optional<models::User> user = users.find_by_email(body.email);
                if (!user) {
                    throw http::ApiError(http::ApiErrorKind::kNotFound,
                                         "no account with that email");
                }
                if (user->role != "trainee") {
                    throw http::ApiError(http::ApiErrorKind::kInvalidInput,
                                         "that account is not a trainee");
                }
                if (!roster.link(claims.user_id, user->id)) {
                    throw http::ApiError(http::ApiErrorKind::kConflict,
                                         "that trainee is already on your roster");
                }
                return dto::user_response(201, *user);
            } catch (const std::exception& ex) {
                return http::problem_response_for(ex);
            }
        });

    // DELETE /api/trainees/<int> ------------------------------------------
    CROW_ROUTE(app, "/api/trainees/<int>")
        .methods(crow::HTTPMethod::Delete)(
            [&app, &roster](const crow::request& req, int trainee_id) {
                try {
                    const auto& ctx = app.template get_context<middleware::JwtAuthMiddleware>(req);
                    const util::TokenClaims claims = http::require_role(ctx, "coach");

                    if (!roster.unlink(claims.user_id, trainee_id)) {
                        throw http::ApiError(http::ApiErrorKind::kNotFound,
                                             "that trainee is not on your roster");
                    }
                    return crow::response(204);
                } catch (const std::exception& ex) {
                    return http::problem_response_for(ex);
                }
            });
}

}  // namespace fitplan::controllers
