#pragma once

#include <stdexcept>
#include <string>

namespace fitplan::services {

// Why a plan operation was refused. Same design as AuthError: the service layer
// speaks in these kinds, and the HTTP layer maps each to a status code. No Crow
// or JSON types reach this far down.
enum class PlanErrorKind {
    kInvalidInput,  // 400 - blank name, no items, a non-positive target value
    kNotFound,      // 404 - no such plan, or it belongs to another coach
    kForbidden,     // 403 - trainee not on the roster, or an item's exercise
                    //       is not in this coach's library
};

class PlanError : public std::runtime_error {
public:
    PlanError(PlanErrorKind kind, const std::string& message)
        : std::runtime_error(message), kind_(kind) {}

    [[nodiscard]] PlanErrorKind kind() const { return kind_; }

private:
    PlanErrorKind kind_;
};

}  // namespace fitplan::services
