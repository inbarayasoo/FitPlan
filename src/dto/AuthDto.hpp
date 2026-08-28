#pragma once

#include <crow.h>

#include <string>

#include "models/User.hpp"
#include "services/AuthService.hpp"

namespace fitplan::dto {

// Plain request shapes, parsed from the JSON body. Field presence/type is
// checked here; business rules (role values, password length) stay in AuthService.
struct RegisterRequest {
    std::string email;
    std::string password;
    std::string role;
    std::string display_name;
};

struct LoginRequest {
    std::string email;
    std::string password;
};

// Parse helpers. Throw services::AuthError(kInvalidInput) on malformed JSON or a
// missing/!string field.
RegisterRequest parse_register_request(const std::string& body);
LoginRequest parse_login_request(const std::string& body);

// Response builders. `auth_response` is the { access_token, user } body shared by
// register and login; `user_response` is the bare user object for /auth/me.
crow::response auth_response(int status, const services::AuthOutcome& outcome);
crow::response user_response(int status, const models::User& user);

}  // namespace fitplan::dto
