#pragma once

#include <cstdint>
#include <string>

#include "models/User.hpp"
#include "repositories/UserRepository.hpp"

namespace fitplan::services {

// The result of a successful register or login: the stored user plus a freshly
// signed access token for it.
struct AuthOutcome {
    models::User user;
    std::string access_token;
};

// Registration, login, and "who does this token belong to" - the rules, with no
// HTTP or JSON in sight. Borrows the UserRepository; owns its JWT settings.
class AuthService {
public:
    AuthService(repositories::UserRepository& users, std::string jwt_secret,
                std::int64_t jwt_ttl_seconds);

    // Hashes the password and stores a new user. Throws AuthError:
    //   kInvalidInput     - empty field, role not trainee/coach, password < 8
    //   kEmailAlreadyUsed - email already registered
    AuthOutcome register_user(const std::string& email, const std::string& password,
                              const std::string& role, const std::string& display_name);

    // Verifies the password against the stored hash. Throws AuthError:
    //   kInvalidCredentials - no such email, or wrong password
    AuthOutcome login(const std::string& email, const std::string& password);

    // Loads the user named by a verified token's subject. Throws AuthError:
    //   kInvalidCredentials - the user no longer exists
    models::User authenticated_user(std::int64_t user_id);

private:
    std::string sign_token_for(const models::User& user) const;

    repositories::UserRepository& users_;
    std::string jwt_secret_;
    std::int64_t jwt_ttl_seconds_;
};

}  // namespace fitplan::services