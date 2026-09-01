#include "services/AuthService.hpp"

#include <cstddef>
#include <utility>

#include "services/AuthError.hpp"
#include "util/Hashing.hpp"
#include "util/Jwt.hpp"

namespace fitplan::services {

namespace {

constexpr std::size_t kMinPasswordLength = 8;

bool is_valid_role(const std::string& role) {
    return role == "trainee" || role == "coach";
}

}  // namespace

AuthService::AuthService(repositories::UserRepository& users, std::string jwt_secret,
                         std::int64_t jwt_ttl_seconds)
    : users_(users), jwt_secret_(std::move(jwt_secret)), jwt_ttl_seconds_(jwt_ttl_seconds) {}

AuthOutcome AuthService::register_user(const std::string& email, const std::string& password,
                                       const std::string& role, const std::string& display_name) {
    if (email.empty() || password.empty() || display_name.empty()) {
        throw AuthError(AuthErrorKind::kInvalidInput,
                        "email, password and display_name are required");
    }
    if (!is_valid_role(role)) {
        throw AuthError(AuthErrorKind::kInvalidInput, "role must be 'trainee' or 'coach'");
    }
    if (password.size() < kMinPasswordLength) {
        throw AuthError(AuthErrorKind::kInvalidInput, "password must be at least 8 characters");
    }
    if (users_.email_exists(email)) {
        throw AuthError(AuthErrorKind::kEmailAlreadyUsed, "email is already registered");
    }

    models::User to_create;
    to_create.email = email;
    to_create.password_hash = util::hash_password(password);
    to_create.role = role;
    to_create.display_name = display_name;

    const models::User created = users_.create(to_create);
    return {created, sign_token_for(created)};
}

AuthOutcome AuthService::login(const std::string& email, const std::string& password) {
    const auto user = users_.find_by_email(email);
    if (!user.has_value() || !util::verify_password(user->password_hash, password)) {
        throw AuthError(AuthErrorKind::kInvalidCredentials, "invalid email or password");
    }
    return {*user, sign_token_for(*user)};
}

models::User AuthService::authenticated_user(std::int64_t user_id) {
    const auto user = users_.find_by_id(user_id);
    if (!user.has_value()) {
        throw AuthError(AuthErrorKind::kInvalidCredentials, "account no longer exists");
    }
    return *user;
}

std::string AuthService::sign_token_for(const models::User& user) const {
    return util::make_access_token(user.id, user.role, jwt_secret_, jwt_ttl_seconds_);
}

}  // namespace fitplan::services