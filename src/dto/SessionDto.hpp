#pragma once

#include <crow.h>

#include <string>
#include <vector>

#include "services/SessionService.hpp"

namespace fitplan::dto {

// Parse + shallow-validate the body of POST /api/my/sessions into the Crow-free
// input the service expects. Throws http::ApiError(kInvalidInput) on malformed
// JSON, a non-integer exercise_id, or a wrong-typed field. Deeper rules
// (status value, exercise exists, plan-item link) belong to SessionService.
services::SessionInput parse_session_request(const std::string& body);

// Parse the body of PATCH /api/my/sessions/{id}. Only the keys present are set.
// Sending "sets" replaces the whole set list (each set carries its own note).
services::SessionPatch parse_session_patch(const std::string& body);

// One session with its sets as JSON.
crow::response session_response(int status, const services::SessionWithSets& s);

// { "sessions": [ <session>, ... ] } - each with its sets.
crow::response session_list_response(const std::vector<services::SessionWithSets>& sessions);

}  // namespace fitplan::dto
