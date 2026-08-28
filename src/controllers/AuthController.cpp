#include "controllers/AuthController.hpp"

#include <crow.h>

#include <exception>

#include "dto/AuthDto.hpp"
#include "http/AuthGuard.hpp"
#include "http/Problem.hpp"
#include "middleware/JwtAuthMiddleware.hpp"
#include "models/User.hpp"
#include "services/AuthError.hpp"
#include "util/Jwt.hpp"

namespace fitplan::controllers {

namespace {

// Maps a domain error to its HTTP status + a problem+json body.
crow::response problem_from(const services::AuthError& err) {
    switch (err.kind()) {
        case services::AuthErrorKind::kInvalidInput:
            // Crow 1.2.x has no 422 in its status table (it would be rewritten to
            // 500 on the wire), so validation failures use 400.
            return http::problem_response(400, "Invalid request", err.what());
        case services::AuthErrorKind::kEmailAlreadyUsed:
            return http::problem_response(409, "Email already registered", err.what());
        case services::AuthErrorKind::kInvalidCredentials:
            return http::problem_response(401, "Authentication failed", err.what());
        case services::AuthErrorKind::kForbidden:
            return http::problem_response(403, "Forbidden", err.what());
    }
    return http::problem_response(500, "Internal server error", "unhandled error kind");
}

}  // namespace

void register_auth_routes(app::FitPlanApp& app, services::AuthService& auth) {
    CROW_ROUTE(app, "/api/auth/register")
        .methods(crow::HTTPMethod::Post)([&auth](const crow::request& req) {
            try {
                const dto::RegisterRequest body =
                    dto::parse_register_request(req.body);
                const services::AuthOutcome outcome = auth.register_user(
                    body.email, body.password, body.role, body.display_name);
                return dto::auth_response(201, outcome);
            } catch (const services::AuthError& err) {
                return problem_from(err);
            } catch (const std::exception& ex) {
                return http::problem_response(500, "Internal server error", ex.what());
            }
        });

    CROW_ROUTE(app, "/api/auth/login")
        .methods(crow::HTTPMethod::Post)([&auth](const crow::request& req) {
            try {
                const dto::LoginRequest body = dto::parse_login_request(req.body);
                const services::AuthOutcome outcome =
                    auth.login(body.email, body.password);
                return dto::auth_response(200, outcome);
            } catch (const services::AuthError& err) {
                return problem_from(err);
            } catch (const std::exception& ex) {
                return http::problem_response(500, "Internal server error", ex.what());
            }
        });

    CROW_ROUTE(app, "/api/auth/me")([&auth, &app](const crow::request& req) {
        try {
            const auto& ctx =
                app.template get_context<middleware::JwtAuthMiddleware>(req);
            const util::TokenClaims claims = http::require_auth(ctx);
            const models::User user = auth.authenticated_user(claims.user_id);
            return dto::user_response(200, user);
        } catch (const services::AuthError& err) {
            return problem_from(err);
        } catch (const std::exception& ex) {
            return http::problem_response(500, "Internal server error", ex.what());
        }
    });
}

}  // namespace fitplan::controllers
