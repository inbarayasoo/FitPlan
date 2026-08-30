#include "controllers/ExerciseController.hpp"

#include <crow.h>

#include <cstdint>
#include <exception>
#include <optional>
#include <vector>

#include "dto/ExerciseDto.hpp"
#include "http/ApiError.hpp"
#include "http/AuthGuard.hpp"
#include "http/Problem.hpp"
#include "middleware/JwtAuthMiddleware.hpp"
#include "models/Exercise.hpp"
#include "util/Jwt.hpp"

namespace fitplan::controllers {

namespace {

models::Exercise owned_or_404(repositories::ExerciseRepository& repo,
                              std::int64_t id, std::int64_t coach_id) {
    const std::optional<models::Exercise> found = repo.find_by_id(id);
    if (!found || found->coach_id != coach_id) {
        throw http::ApiError(http::ApiErrorKind::kNotFound,
                             "no exercise with that id");
    }
    return *found;
}

}  // namespace

void register_exercise_routes(app::FitPlanApp& app,
                              repositories::ExerciseRepository& exercises) {
    // POST /api/exercises -------------------------------------------------------
    CROW_ROUTE(app, "/api/exercises")
        .methods(crow::HTTPMethod::Post)([&app, &exercises](const crow::request& req) {
            try {
                const auto& ctx =
                app.template get_context<middleware::JwtAuthMiddleware>(req);
                const util::TokenClaims claims = http::require_role(ctx, "coach");

                const dto::ExerciseRequest body = dto::parse_exercise_request(req.body);

                models::Exercise to_create;
                to_create.coach_id = claims.user_id;
                to_create.name = body.name;
                to_create.category = body.category;
                to_create.primary_muscle = body.primary_muscle;
                to_create.description = body.description;
                to_create.video_url = body.video_url;

                const models::Exercise created = exercises.create(to_create);
                return dto::exercise_response(201, created);
            } catch (const std::exception& ex) {
                return http::problem_response_for(ex);
            }
        });

    // GET /api/exercises ------------------------------------------------------
    CROW_ROUTE(app, "/api/exercises")([&app, &exercises](const crow::request& req) {
        try {
            const auto& ctx =
            app.template get_context<middleware::JwtAuthMiddleware>(req);
            const util::TokenClaims claims = http::require_role(ctx, "coach");

            const std::vector<models::Exercise> rows =
            exercises.list_by_coach(claims.user_id);
            return dto::exercise_list_response(rows);
        } catch (const std::exception& ex) {
            return http::problem_response_for(ex);
        }
    });

    // GET /api/exercises/<int> ----------------------------------------------
    CROW_ROUTE(app, "/api/exercises/<int>")
        ([&app, &exercises](const crow::request& req, int id) {
            try {
                const auto& ctx =
                app.template get_context<middleware::JwtAuthMiddleware>(req);
                const util::TokenClaims claims = http::require_role(ctx, "coach");

                const models::Exercise e = owned_or_404(exercises, id, claims.user_id);
                return dto::exercise_response(200, e);
            } catch (const std::exception& ex) {
                return http::problem_response_for(ex);
            }
        });

    // PUT /api/exercises/<int> --------------------------------------------------
    CROW_ROUTE(app, "/api/exercises/<int>")
        .methods(crow::HTTPMethod::Put)(
            [&app, &exercises](const crow::request& req, int id) {
                try {
                    const auto& ctx =
                    app.template get_context<middleware::JwtAuthMiddleware>(req);
                    const util::TokenClaims claims = http::require_role(ctx, "coach");

                    owned_or_404(exercises, id, claims.user_id);

                    const dto::ExerciseRequest body = dto::parse_exercise_request(req.body);

                    models::Exercise to_update;
                    to_update.id = id;
                    to_update.coach_id = claims.user_id;
                    to_update.name = body.name;
                    to_update.category = body.category;
                    to_update.primary_muscle = body.primary_muscle;
                    to_update.description = body.description;
                    to_update.video_url = body.video_url;

                    exercises.update(to_update);

                    const std::optional<models::Exercise> fresh = exercises.find_by_id(id);
                    return dto::exercise_response(200, *fresh);
                } catch (const std::exception& ex) {
                    return http::problem_response_for(ex);
                }
            });

    // DELETE /api/exercises/<int> ---------------------------------------------
    CROW_ROUTE(app, "/api/exercises/<int>")
        .methods(crow::HTTPMethod::Delete)(
            [&app, &exercises](const crow::request& req, int id) {
                try {
                    const auto& ctx =
                    app.template get_context<middleware::JwtAuthMiddleware>(req);
                    const util::TokenClaims claims = http::require_role(ctx, "coach");

                    owned_or_404(exercises, id, claims.user_id);
                    exercises.remove(id);
                    return crow::response(204);
                    
                } catch (const std::exception& ex) {
                    return http::problem_response_for(ex);
                }
            });
}

}  // namespace fitplan::controllers
