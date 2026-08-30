#pragma once

#include <crow.h>

#include <string>
#include <vector>

#include "models/User.hpp"

namespace fitplan::dto {

// Body of POST /api/trainees: { "email": "..." }.
struct AttachTraineeRequest {
    std::string email;
};

// Throws http::ApiError(kInvalidInput) on malformed JSON or a missing / blank
// "email".
AttachTraineeRequest parse_attach_trainee_request(const std::string& body);

// { "trainees": [ <user>, ... ] } - each user without password_hash.
crow::response trainee_list_response(const std::vector<models::User>& trainees);

}  // namespace fitplan::dto
