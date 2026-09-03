#include "controllers/AuthController.hpp"

#include <crow.h>

#include <exception>
#include <string>

#include "dto/AuthDto.hpp"
#include "http/AuthGuard.hpp"
#include "http/Problem.hpp"
#include "middleware/JwtAuthMiddleware.hpp"
#include "models/User.hpp"
#include "services/AuthError.hpp"
#include "services/EmailVerificationError.hpp"
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
        case services::AuthErrorKind::kEmailNotVerified:
            return http::problem_response(403, "Email not verified", err.what());
    }
    return http::problem_response(500, "Internal server error", "unhandled error kind");
}

crow::response problem_from(const services::EmailVerificationError& err) {
    switch (err.kind()) {
        case services::EmailVerificationErrorKind::kNotPending:
            return http::problem_response(404, "No verification pending", err.what());
        case services::EmailVerificationErrorKind::kCodeExpired:
            return http::problem_response(410, "Code expired", err.what());
        case services::EmailVerificationErrorKind::kTooManyAttempts:
            return http::problem_response(429, "Too many attempts", err.what());
        case services::EmailVerificationErrorKind::kCodeMismatch:
            return http::problem_response(400, "Incorrect code", err.what());
    }
    return http::problem_response(500, "Internal server error", "unhandled error kind");
}

// Extracted so register_auth_routes stays a flat list of route registrations
// (each route's own try/catch would otherwise pile up its cognitive complexity).
crow::response handle_verify_email(services::AuthService& auth, const crow::request& req) {
    try {
        const dto::VerifyEmailRequest body = dto::parse_verify_email_request(req.body);
        return dto::auth_response(200, auth.verify_email(body.email, body.code));
    } catch (const services::EmailVerificationError& err) {
        return problem_from(err);
    } catch (const services::AuthError& err) {
        return problem_from(err);
    } catch (const std::exception& ex) {
        return http::problem_response(500, "Internal server error", ex.what());
    }
}

crow::response handle_resend_verification(services::AuthService& auth, const crow::request& req) {
    try {
        const dto::ResendVerificationRequest body =
            dto::parse_resend_verification_request(req.body);
        auth.resend_verification(body.email);
        return dto::accepted_response();
    } catch (const services::AuthError& err) {
        return problem_from(err);
    } catch (const std::exception& ex) {
        return http::problem_response(500, "Internal server error", ex.what());
    }
}

}  // namespace

void register_auth_routes(app::FitPlanApp& app, services::AuthService& auth,
                          const std::string& google_client_id) {
    CROW_ROUTE(app, "/api/auth/register")
        .methods(crow::HTTPMethod::Post)([&auth](const crow::request& req) {
            try {
                const dto::RegisterRequest body = dto::parse_register_request(req.body);
                const services::RegisterOutcome outcome =
                    auth.register_user(body.email, body.password, body.role, body.display_name);
                return dto::registered_response(outcome);
            } catch (const services::AuthError& err) {
                return problem_from(err);
            } catch (const std::exception& ex) {
                return http::problem_response(500, "Internal server error", ex.what());
            }
        });

    CROW_ROUTE(app, "/api/auth/verify-email")
        .methods(crow::HTTPMethod::Post)(
            [&auth](const crow::request& req) { return handle_verify_email(auth, req); });

    CROW_ROUTE(app, "/api/auth/resend-verification")
        .methods(crow::HTTPMethod::Post)(
            [&auth](const crow::request& req) { return handle_resend_verification(auth, req); });

    CROW_ROUTE(app, "/api/auth/login")
        .methods(crow::HTTPMethod::Post)([&auth](const crow::request& req) {
            try {
                const dto::LoginRequest body = dto::parse_login_request(req.body);
                const services::AuthOutcome outcome = auth.login(body.email, body.password);
                return dto::auth_response(200, outcome);
            } catch (const services::AuthError& err) {
                return problem_from(err);
            } catch (const std::exception& ex) {
                return http::problem_response(500, "Internal server error", ex.what());
            }
        });

    CROW_ROUTE(app, "/api/auth/me")
    ([&auth, &app](const crow::request& req) {
        try {
            const auto& ctx = app.template get_context<middleware::JwtAuthMiddleware>(req);
            const util::TokenClaims claims = http::require_auth(ctx);
            const models::User user = auth.authenticated_user(claims.user_id);
            return dto::user_response(200, user);
        } catch (const services::AuthError& err) {
            return problem_from(err);
        } catch (const std::exception& ex) {
            return http::problem_response(500, "Internal server error", ex.what());
        }
    });

    // Public: lets the frontend decide whether to show the Google button.
    CROW_ROUTE(app, "/api/auth/config")
    ([google_client_id]() { return dto::config_response(google_client_id); });

    // Always registered. When Google Sign-In is not configured,
    // AuthService::login_with_google throws kInvalidInput -> 400, rather than the
    // route 404ing.
    CROW_ROUTE(app, "/api/auth/google")
        .methods(crow::HTTPMethod::Post)([&auth](const crow::request& req) {
            try {
                const dto::GoogleLoginRequest body = dto::parse_google_login_request(req.body);
                const services::GoogleLoginResult result =
                    auth.login_with_google(body.id_token, body.role);
                if (result.needs_role) {
                    return dto::role_needed_response();
                }
                return dto::auth_response(200, result.outcome);
            } catch (const services::AuthError& err) {
                return problem_from(err);
            } catch (const std::exception& ex) {
                return http::problem_response(500, "Internal server error", ex.what());
            }
        });
}

}  // namespace fitplan::controllers
