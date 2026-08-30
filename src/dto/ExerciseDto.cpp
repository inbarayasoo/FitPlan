#include "dto/ExerciseDto.hpp"

#include <nlohmann/json.hpp>

#include "http/ApiError.hpp"
#include "http/Json.hpp"
#include "util/Validation.hpp"

namespace fitplan::dto {

namespace {

using nlohmann::json;

[[noreturn]] void invalid(const std::string& detail) {
    throw http::ApiError(http::ApiErrorKind::kInvalidInput, detail);
}

json parse_object_or_throw(const std::string& body) {
    const json parsed = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (!parsed.is_object()) {
        invalid("request body must be a JSON object");
    }
    return parsed;
}

// A required, non-blank string field.
std::string required_string(const json& obj, const char* key) {
    if (!obj.contains(key) || !obj.at(key).is_string()) {
        invalid(std::string("missing or non-string field: ") + key);
    }
    std::string value = obj.at(key).get<std::string>();
    if (value.find_first_not_of(" \t\r\n") == std::string::npos) {
        invalid(std::string("field must not be blank: ") + key);
    }
    return value;
}

// An optional string field: absent, JSON null, or "" all map to std::nullopt;
// a present non-string value is an error.
std::optional<std::string> optional_string(const json& obj, const char* key) {
    if (!obj.contains(key) || obj.at(key).is_null()) {
        return std::nullopt;
    }
    if (!obj.at(key).is_string()) {
        invalid(std::string("field must be a string or null: ") + key);
    }
    std::string value = obj.at(key).get<std::string>();
    if (value.empty()) {
        return std::nullopt;
    }
    return value;
}

json exercise_to_json(const models::Exercise& e) {
    json out{
        {"id", e.id},
        {"coach_id", e.coach_id},
        {"name", e.name},
        {"category", nullptr},
        {"primary_muscle", nullptr},
        {"description", nullptr},
        {"video_url", nullptr},
        {"video_embed_url", nullptr},
        {"created_at", e.created_at},
    };
    if (e.category) out["category"] = *e.category;
    if (e.primary_muscle) out["primary_muscle"] = *e.primary_muscle;
    if (e.description) out["description"] = *e.description;
    if (e.video_url) {
        out["video_url"] = *e.video_url;
        if (const auto embed = util::youtube_embed_url(*e.video_url)) {
            out["video_embed_url"] = *embed;
        }
    }
    return out;
}

}  // namespace

ExerciseRequest parse_exercise_request(const std::string& body) {
    const json obj = parse_object_or_throw(body);

    ExerciseRequest req;
    req.name = required_string(obj, "name");
    req.category = optional_string(obj, "category");
    req.primary_muscle = optional_string(obj, "primary_muscle");
    req.description = optional_string(obj, "description");
    req.video_url = optional_string(obj, "video_url");

    if (req.video_url && !util::is_allowed_video_url(*req.video_url)) {
        invalid(
            "video_url must be an https link to youtube.com, youtu.be, or "
            "instagram.com");
    }
    return req;
}

crow::response exercise_response(int status, const models::Exercise& exercise) {
    return http::json_response(status, exercise_to_json(exercise));
}

crow::response exercise_list_response(const std::vector<models::Exercise>& items) {
    json arr = json::array();
    for (const models::Exercise& e : items) {
        arr.push_back(exercise_to_json(e));
    }
    return http::json_response(200, json{{"exercises", std::move(arr)}});
}

}  // namespace fitplan::dto
