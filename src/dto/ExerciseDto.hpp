#pragma once

#include <crow.h>

#include <optional>
#include <string>
#include <vector>

#include "models/Exercise.hpp"

namespace fitplan::dto {

// Body of POST /api/exercises and PUT /api/exercises/{id}. `coach_id`, `id`, and
// `created_at` are never taken from the body - the first comes from the token,
// the other two from the database.
struct ExerciseRequest {
    std::string name;
    std::optional<std::string> category;
    std::optional<std::string> primary_muscle;
    std::optional<std::string> description;
    std::optional<std::string> video_url;
};

// Parse + shallow-validate the JSON body. Throws http::ApiError(kInvalidInput)
// on: malformed JSON, a missing / blank / non-string "name", a present-but-
// non-string optional field, or a "video_url" that is not an https YouTube /
// Instagram link (per util::is_allowed_video_url).
ExerciseRequest parse_exercise_request(const std::string& body);

// One exercise as JSON. Nullable columns serialize as JSON null. `video_embed_url`
// is the youtube-nocookie embed form when `video_url` is a YouTube link, else null.
crow::response exercise_response(int status, const models::Exercise& exercise);

// { "exercises": [ ... ] } - the list endpoint's envelope.
crow::response exercise_list_response(const std::vector<models::Exercise>& items);

}  // namespace fitplan::dto
