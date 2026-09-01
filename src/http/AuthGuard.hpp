#pragma once

#include <string>

#include "middleware/JwtAuthMiddleware.hpp"
#include "services/AuthError.hpp"
#include "util/Jwt.hpp"

namespace fitplan::http {

// Route-side authorization helpers. The middleware only *extracts* the token;
// these decide whether a given route accepts the request. They throw AuthError,
// which the controller turns into a problem+json response.

// Requires a valid access token. Returns its claims, or throws
// kInvalidCredentials (-> 401) when the request had none.
inline util::TokenClaims require_auth(const middleware::JwtAuthMiddleware::context& ctx) {
    if (!ctx.claims.has_value()) {
        throw services::AuthError(services::AuthErrorKind::kInvalidCredentials,
                                  "a valid access token is required");
    }
    return *ctx.claims;
}

// Requires a valid token *and* a specific role. Throws kForbidden (-> 403) when
// the caller is authenticated but has the wrong role.
inline util::TokenClaims require_role(const middleware::JwtAuthMiddleware::context& ctx,
                                      const std::string& role) {
    const util::TokenClaims claims = require_auth(ctx);
    if (claims.role != role) {
        throw services::AuthError(services::AuthErrorKind::kForbidden,
                                  "this action requires the '" + role + "' role");
    }
    return claims;
}

}  // namespace fitplan::http
