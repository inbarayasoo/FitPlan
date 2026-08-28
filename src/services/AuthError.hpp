#pragma once

#include <stdexcept>
#include <string>

namespace fitplan::services {

// Why a register/login attempt was refused. The HTTP layer maps each value to a
// status code; the service layer never mentions HTTP.
enum class AuthErrorKind {
    kInvalidInput,        // missing field, bad role, password too short
    kEmailAlreadyUsed,    // registration with a taken email
    kInvalidCredentials,  // wrong email or password, or missing/expired token
    kForbidden,           // authenticated, but the role is not allowed here
};

// Thrown by AuthService for an expected, client-caused failure. Unexpected
// errors (SQLite::Exception, ...) are left to propagate untouched.
class AuthError : public std::runtime_error {
public:
    AuthError(AuthErrorKind kind, const std::string& message)
        : std::runtime_error(message), kind_(kind) {}

    [[nodiscard]] AuthErrorKind kind() const { return kind_; }

private:
    AuthErrorKind kind_;
};

}  // namespace fitplan::services