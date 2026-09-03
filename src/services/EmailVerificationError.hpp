#pragma once

#include <stdexcept>
#include <string>

namespace fitplan::services {

// Why an email-verification attempt was refused. Same design as AuthError: the
// service layer speaks in these kinds, and the HTTP layer maps each to a status
// code. No Crow or JSON types reach this far down.
enum class EmailVerificationErrorKind {
    kNotPending,       // 404 - no code is outstanding for this address
    kCodeExpired,      // 410 - the code's lifetime has passed; request a new one
    kTooManyAttempts,  // 429 - the guess limit was reached; the code is now dead
    kCodeMismatch,     // 400 - wrong code (this guess has been counted)
};

class EmailVerificationError : public std::runtime_error {
public:
    EmailVerificationError(EmailVerificationErrorKind kind, const std::string& message)
        : std::runtime_error(message), kind_(kind) {}

    [[nodiscard]] EmailVerificationErrorKind kind() const { return kind_; }

private:
    EmailVerificationErrorKind kind_;
};

}  // namespace fitplan::services
