#pragma once

#include <stdexcept>
#include <string>

namespace fitplan::services {

// Why a session (trainee-side) operation was refused. Same design as AuthError
// and PlanError: the service layer speaks in these kinds, the HTTP layer maps
// each to a status code. No Crow or JSON types reach this far down.
enum class SessionErrorKind {
    kInvalidInput,  // 400 - bad status value, unknown exercise, negative number
    kNotFound,      // 404 - no such session (or it is another trainee's), or the
                    //       trainee has no active plan
    kForbidden,     // 403 - a set links to a plan item that is not on the
                    //       trainee's active plan
};

class SessionError : public std::runtime_error {
public:
    SessionError(SessionErrorKind kind, const std::string& message)
        : std::runtime_error(message), kind_(kind) {}

    [[nodiscard]] SessionErrorKind kind() const { return kind_; }

private:
    SessionErrorKind kind_;
};

}  // namespace fitplan::services
