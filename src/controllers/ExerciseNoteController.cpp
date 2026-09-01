#include "controllers/ExerciseNoteController.hpp"

#include <crow.h>

#include <cstdint>
#include <exception>
#include <optional>

#include "dto/ExerciseNoteDto.hpp"
#include "http/ApiError.hpp"
#include "http/AuthGuard.hpp"
#include "middleware/JwtAuthMiddleware.hpp"
#include "models/PlanItem.hpp"
#include "models/WorkoutPlan.hpp"
#include "util/Jwt.hpp"

namespace fitplan::controllers {

namespace {

// The note can only be attached to an exercise the trainee is actually training,
// i.e. one that appears on their active plan. Throws http::ApiError otherwise.
void require_exercise_on_active_plan(repositories::PlanRepository& plans,
                                     repositories::PlanItemRepository& plan_items,
                                     std::int64_t trainee_id, std::int64_t exercise_id) {
    const std::optional<models::WorkoutPlan> plan = plans.find_active_for_trainee(trainee_id);
    if (!plan) {
        throw http::ApiError(http::ApiErrorKind::kNotFound, "you have no active plan");
    }
    for (const models::PlanItem& item : plan_items.list_by_plan(plan->id)) {
        if (item.exercise_id == exercise_id) {
            return;
        }
    }
    throw http::ApiError(http::ApiErrorKind::kNotFound, "that exercise is not on your active plan");
}

}  // namespace

void register_exercise_note_routes(app::FitPlanApp& app,
                                   repositories::ExerciseNoteRepository& notes,
                                   repositories::PlanRepository& plans,
                                   repositories::PlanItemRepository& plan_items) {
    // GET /api/my/notes -------------------------------------------------------
    CROW_ROUTE(app, "/api/my/notes")
    ([&app, &notes](const crow::request& req) {
        try {
            const auto& ctx = app.template get_context<middleware::JwtAuthMiddleware>(req);
            const util::TokenClaims claims = http::require_role(ctx, "trainee");
            return dto::note_list_response(notes.list_for_trainee(claims.user_id));
        } catch (const std::exception& ex) {
            return http::problem_response_for(ex);
        }
    });

    // PUT /api/my/notes/<int> ----------------------------------------------
    CROW_ROUTE(app, "/api/my/notes/<int>")
        .methods(crow::HTTPMethod::Put)(
            [&app, &notes, &plans, &plan_items](const crow::request& req, int exercise_id) {
                try {
                    const auto& ctx = app.template get_context<middleware::JwtAuthMiddleware>(req);
                    const util::TokenClaims claims = http::require_role(ctx, "trainee");

                    const std::string body = dto::parse_note_request(req.body);
                    require_exercise_on_active_plan(plans, plan_items, claims.user_id, exercise_id);
                    return dto::note_response(200, notes.upsert(claims.user_id, exercise_id, body));
                } catch (const std::exception& ex) {
                    return http::problem_response_for(ex);
                }
            });

    // DELETE /api/my/notes/<int> -----------------------------------------
    CROW_ROUTE(app, "/api/my/notes/<int>")
        .methods(crow::HTTPMethod::Delete)(
            [&app, &notes](const crow::request& req, int exercise_id) {
                try {
                    const auto& ctx = app.template get_context<middleware::JwtAuthMiddleware>(req);
                    const util::TokenClaims claims = http::require_role(ctx, "trainee");

                    if (!notes.remove(claims.user_id, exercise_id)) {
                        throw http::ApiError(http::ApiErrorKind::kNotFound,
                                             "no note for that exercise");
                    }
                    return crow::response(204);
                } catch (const std::exception& ex) {
                    return http::problem_response_for(ex);
                }
            });
}

}  // namespace fitplan::controllers
