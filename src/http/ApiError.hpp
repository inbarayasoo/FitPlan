#pragma once

#include <crow.h>

#include <exception>
#include <stdexcept>
#include <string>

#include "http/Problem.hpp"
#include "services/AuthError.hpp"
#include "services/PlanError.hpp"
#include "services/SessionError.hpp"

namespace fitplan::http {

// A client-caused failure raised anywhere below the HTTP layer of a resource
// controller (DTO parsing, ownership checks, repository lookups that come back
// empty). The controller's single catch block hands it to problem_response_for()
// which turns the kind into a status code. Unexpected errors (SQLite::Exception,
// std::bad_alloc, ...) are deliberately NOT of this type, so they fall through to
// a 500.
enum class ApiErrorKind {
    kInvalidInput,  // 400 - malformed body or a bad field value
    kNotFound,      // 404 - no such row, or it belongs to another coach
    kForbidden,     // 403 - authenticated, but not allowed to do this
    kConflict,      // 409 - would break a uniqueness / already-linked rule
};

class ApiError : public std::runtime_error {
public:
    ApiError(ApiErrorKind kind, const std::string& message)
        : std::runtime_error(message), kind_(kind) {}

    [[nodiscard]] ApiErrorKind kind() const { return kind_; }

private:
    ApiErrorKind kind_;
};

// --- exception -> application/problem+json -------------------------------------

inline crow::response problem_response_for(const ApiError& err) {
    switch (err.kind()) {
        case ApiErrorKind::kInvalidInput:
            return problem_response(400, "Invalid request", err.what());
        case ApiErrorKind::kNotFound:
            return problem_response(404, "Not found", err.what());
        case ApiErrorKind::kForbidden:
            return problem_response(403, "Forbidden", err.what());
        case ApiErrorKind::kConflict:
            return problem_response(409, "Conflict", err.what());
    }
    return problem_response(500, "Internal server error", "unhandled error kind");
}

inline crow::response problem_response_for(const services::AuthError& err) {
    switch (err.kind()) {
        case services::AuthErrorKind::kInvalidInput:
            return problem_response(400, "Invalid request", err.what());
        case services::AuthErrorKind::kEmailAlreadyUsed:
            return problem_response(409, "Email already registered", err.what());
        case services::AuthErrorKind::kInvalidCredentials:
            return problem_response(401, "Authentication failed", err.what());
        case services::AuthErrorKind::kForbidden:
            return problem_response(403, "Forbidden", err.what());
    }
    return problem_response(500, "Internal server error", "unhandled error kind");
}

inline crow::response problem_response_for(const services::PlanError& err) {
    switch (err.kind()) {
        case services::PlanErrorKind::kInvalidInput:
            return problem_response(400, "Invalid request", err.what());
        case services::PlanErrorKind::kNotFound:
            return problem_response(404, "Not found", err.what());
        case services::PlanErrorKind::kForbidden:
            return problem_response(403, "Forbidden", err.what());
    }
    return problem_response(500, "Internal server error", "unhandled error kind");
}

inline crow::response problem_response_for(const services::SessionError& err) {
    switch (err.kind()) {
        case services::SessionErrorKind::kInvalidInput:
            return problem_response(400, "Invalid request", err.what());
        case services::SessionErrorKind::kNotFound:
            return problem_response(404, "Not found", err.what());
        case services::SessionErrorKind::kForbidden:
            return problem_response(403, "Forbidden", err.what());
    }
    return problem_response(500, "Internal server error", "unhandled error kind");
}

// The one call a route's `catch (const std::exception&)` needs: dispatch on the
// dynamic type to the right mapper, or fall back to 500 for anything unforeseen.
inline crow::response problem_response_for(const std::exception& ex) {
    if (const auto* api = dynamic_cast<const ApiError*>(&ex)) {
        return problem_response_for(*api);
    }
    if (const auto* auth = dynamic_cast<const services::AuthError*>(&ex)) {
        return problem_response_for(*auth);
    }
    if (const auto* plan = dynamic_cast<const services::PlanError*>(&ex)) {
        return problem_response_for(*plan);
    }
    if (const auto* session = dynamic_cast<const services::SessionError*>(&ex)) {
        return problem_response_for(*session);
    }
    return problem_response(500, "Internal server error", ex.what());
}

}  // namespace fitplan::http
