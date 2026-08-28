#pragma once

#include <crow.h>

#include <optional>
#include <string>
#include <string_view>

#include "util/Jwt.hpp"

namespace fitplan::middleware {

// Crow middleware: on every request, if there is an
// "Authorization: Bearer <token>" header, verify it and stash the resulting
// claims in the request context. It never rejects a request - each route decides
// for itself whether authentication is required.
struct JwtAuthMiddleware {
    struct context {
        std::optional<util::TokenClaims> claims;
    };

    // Filled once at startup from Config, before app.run().
    std::string secret;

    void before_handle(crow::request& req, crow::response&, context& ctx) {
        const std::string& header = req.get_header_value("Authorization");
        constexpr std::string_view scheme = "Bearer ";
        if (header.starts_with(scheme)) {
            ctx.claims =
                util::verify_access_token(header.substr(scheme.size()), secret);
        }
    }

    void after_handle(crow::request&, crow::response&, context&) {}
};

}  // namespace fitplan::middleware