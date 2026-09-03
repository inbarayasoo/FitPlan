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

struct GoogleLoginRequest {
    std::string id_token;
    std::string role;  // "" | "trainee" | "coach" - only read when creating a new account
};

struct VerifyEmailRequest {
    std::string email;
    std::string code;
};

struct ResendVerificationRequest {
    std::string email;
};

// Parse helpers. Throw services::AuthError(kInvalidInput) on malformed JSON or a
// missing/!string field.
RegisterRequest parse_register_request(const std::string& body);
LoginRequest parse_login_request(const std::string& body);
GoogleLoginRequest parse_google_login_request(const std::string& body);
VerifyEmailRequest parse_verify_email_request(const std::string& body);
ResendVerificationRequest parse_resend_verification_request(const std::string& body);

// Response builders. `auth_response` is the { access_token, user } body shared by
// login, Google login and email verification; `user_response` is the bare user
// object for /auth/me; `registered_response` is the 201 { verification_required,
// user } from register (no token yet); `accepted_response` is the 202 for a
// resend request; `config_response` tells the frontend whether Google Sign-In is
// on; `role_needed_response` is the { needs_role: true } body from Google login.
crow::response auth_response(int status, const services::AuthOutcome& outcome);
crow::response user_response(int status, const models::User& user);
crow::response registered_response(const services::RegisterOutcome& outcome);
crow::response accepted_response();
crow::response config_response(const std::string& google_client_id);
crow::response role_needed_response();

}  // namespace fitplan::dto
