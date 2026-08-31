#pragma once

#include <crow.h>

#include <string>
#include <vector>

#include "models/ExerciseNote.hpp"

namespace fitplan::dto {

// Body of PUT /api/my/notes/<exercise_id>: { "body": "..." }.
// Throws http::ApiError(kInvalidInput) on malformed JSON or a missing / blank
// "body".
std::string parse_note_request(const std::string& body);

// One note as { exercise_id, exercise_name, body, updated_at }.
crow::response note_response(int status, const models::ExerciseNote& note);

// { "notes": [ <note>, ... ] }.
crow::response note_list_response(const std::vector<models::ExerciseNote>& notes);

}  // namespace fitplan::dto
