#include "controllers/PlanController.hpp"

#include <crow.h>

#include <exception>

#include "dto/PlanDto.hpp"
#include "http/ApiError.hpp"
#include "http/AuthGuard.hpp"
#include "middleware/JwtAuthMiddleware.hpp"
#include "util/Jwt.hpp"

namespace fitplan::controllers {

void register_plan_routes(app::FitPlanApp& app, services::PlanService& plans) {
    // POST /api/plans --------------------------------------------------------
    CROW_ROUTE(app, "/api/plans")
        .methods(crow::HTTPMethod::Post)([&app, &plans](const crow::request& req) {
            try {
                const auto& ctx =
                    app.template get_context<middleware::JwtAuthMiddleware>(req);
                const util::TokenClaims claims = http::require_role(ctx, "coach");

                const services::PlanInput input = dto::parse_plan_request(req.body);
                const services::PlanWithItems created =
                    plans.create_plan(claims.user_id, input);
                return dto::plan_response(201, created);
            } catch (const std::exception& ex) {
                return http::problem_response_for(ex);
            }
        });

    // GET /api/plans ------------------------------------------------------------
    CROW_ROUTE(app, "/api/plans")([&app, &plans](const crow::request& req) {
        try {
            const auto& ctx =
                app.template get_context<middleware::JwtAuthMiddleware>(req);
            const util::TokenClaims claims = http::require_role(ctx, "coach");
            return dto::plan_list_response(plans.list_plans(claims.user_id));
        } catch (const std::exception& ex) {
            return http::problem_response_for(ex);
        }
    });

    // GET /api/plans/<int> ----------------------------------------------------
    CROW_ROUTE(app, "/api/plans/<int>")
        ([&app, &plans](const crow::request& req, int id) {
            try {
                const auto& ctx =
                    app.template get_context<middleware::JwtAuthMiddleware>(req);
                const util::TokenClaims claims = http::require_role(ctx, "coach");
                return dto::plan_response(200,
                                         plans.get_plan(claims.user_id, id));
            } catch (const std::exception& ex) {
                return http::problem_response_for(ex);
            }
        });

    // PUT /api/plans/<int> ---------------------------------------------------
    CROW_ROUTE(app, "/api/plans/<int>")
        .methods(crow::HTTPMethod::Put)(
            [&app, &plans](const crow::request& req, int id) {
                try {
                    const auto& ctx =
                        app.template get_context<middleware::JwtAuthMiddleware>(
                            req);
                    const util::TokenClaims claims =
                        http::require_role(ctx, "coach");

                    const services::PlanInput input =
                        dto::parse_plan_request(req.body);
                    return dto::plan_response(
                        200, plans.update_plan(claims.user_id, id, input));
                } catch (const std::exception& ex) {
                    return http::problem_response_for(ex);
                }
            });

    // DELETE /api/plans/<int> ----------------------------------------------
    CROW_ROUTE(app, "/api/plans/<int>")
        .methods(crow::HTTPMethod::Delete)(
            [&app, &plans](const crow::request& req, int id) {
                try {
                    const auto& ctx =
                        app.template get_context<middleware::JwtAuthMiddleware>(
                            req);
                    const util::TokenClaims claims =
                        http::require_role(ctx, "coach");
                    plans.delete_plan(claims.user_id, id);
                    return crow::response(204);
                } catch (const std::exception& ex) {
                    return http::problem_response_for(ex);
                }
            });

    // POST /api/plans/<int>/assign -----------------------------------------
    CROW_ROUTE(app, "/api/plans/<int>/assign")
        .methods(crow::HTTPMethod::Post)(
            [&app, &plans](const crow::request& req, int id) {
                try {
                    const auto& ctx =
                        app.template get_context<middleware::JwtAuthMiddleware>(
                            req);
                    const util::TokenClaims claims =
                        http::require_role(ctx, "coach");
                    return dto::plan_response(
                        200, plans.assign_plan(claims.user_id, id));
                } catch (const std::exception& ex) {
                    return http::problem_response_for(ex);
                }
            });
}

}  // namespace fitplan::controllers
