#include "dto/ExerciseNoteDto.hpp"

#include <nlohmann/json.hpp>

#include "http/ApiError.hpp"
#include "http/Json.hpp"

namespace fitplan::dto {

namespace {

using nlohmann::json;

json note_to_json(const models::ExerciseNote& n) {
    return json{
        {"exercise_id", n.exercise_id},
        {"exercise_name", n.exercise_name},
        {"body", n.body},
        {"updated_at", n.updated_at},
    };
}

}  // namespace

std::string parse_note_request(const std::string& body) {
    const json parsed = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (!parsed.is_object() || !parsed.contains("body") ||
        !parsed.at("body").is_string()) {
        throw http::ApiError(http::ApiErrorKind::kInvalidInput,
                             R"(body must be a JSON object with a string "body")");
    }
    std::string text = parsed.at("body").get<std::string>();
    if (text.find_first_not_of(" \t\r\n") == std::string::npos) {
        throw http::ApiError(http::ApiErrorKind::kInvalidInput,
                             "note body must not be blank");
    }
    return text;
}

crow::response note_response(int status, const models::ExerciseNote& note) {
    return http::json_response(status, note_to_json(note));
}

crow::response note_list_response(
    const std::vector<models::ExerciseNote>& notes) {
    json arr = json::array();
    for (const models::ExerciseNote& n : notes) {
        arr.push_back(note_to_json(n));
    }
    return http::json_response(200, json{{"notes", std::move(arr)}});
}

}  // namespace fitplan::dto
