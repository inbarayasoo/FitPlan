#include "dto/SessionDto.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>

#include "http/ApiError.hpp"
#include "http/Json.hpp"
#include "models/SessionSet.hpp"
#include "models/WorkoutSession.hpp"

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

std::int64_t required_int(const json& obj, const char* key) {
    if (!obj.contains(key) || !obj.at(key).is_number_integer()) {
        invalid(std::string("missing or non-integer field: ") + key);
    }
    return obj.at(key).get<std::int64_t>();
}

std::optional<std::int64_t> optional_id(const json& obj, const char* key) {
    if (!obj.contains(key) || obj.at(key).is_null()) {
        return std::nullopt;
    }
    if (!obj.at(key).is_number_integer()) {
        invalid(std::string("field must be an integer or null: ") + key);
    }
    return obj.at(key).get<std::int64_t>();
}

std::optional<int> optional_int(const json& obj, const char* key) {
    if (!obj.contains(key) || obj.at(key).is_null()) {
        return std::nullopt;
    }
    if (!obj.at(key).is_number_integer()) {
        invalid(std::string("field must be an integer or null: ") + key);
    }
    return obj.at(key).get<int>();
}

std::optional<double> optional_double(const json& obj, const char* key) {
    if (!obj.contains(key) || obj.at(key).is_null()) {
        return std::nullopt;
    }
    if (!obj.at(key).is_number()) {
        invalid(std::string("field must be a number or null: ") + key);
    }
    return obj.at(key).get<double>();
}

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

bool optional_bool(const json& obj, const char* key, bool fallback) {
    if (!obj.contains(key) || obj.at(key).is_null()) {
        return fallback;
    }
    if (!obj.at(key).is_boolean()) {
        invalid(std::string("field must be a boolean: ") + key);
    }
    return obj.at(key).get<bool>();
}

services::SessionSetInput parse_set(const json& obj) {
    if (!obj.is_object()) {
        invalid("each entry of sets must be a JSON object");
    }
    services::SessionSetInput s;
    s.exercise_id = required_int(obj, "exercise_id");
    s.plan_item_id = optional_id(obj, "plan_item_id");
    s.reps = optional_int(obj, "reps");
    s.weight = optional_double(obj, "weight");
    s.rpe = optional_double(obj, "rpe");
    s.completed = optional_bool(obj, "completed", true);
    return s;
}

// --- serialization --------------------------------------------------------

template <class T>
void put_optional(json& out, const char* key, const std::optional<T>& v) {
    if (v.has_value()) {
        out[key] = *v;
    } else {
        out[key] = nullptr;
    }
}

json set_to_json(const models::SessionSet& s) {
    json out{
        {"id", s.id},
        {"session_id", s.session_id},
        {"exercise_id", s.exercise_id},
        {"set_number", s.set_number},
        {"completed", s.completed},
    };
    put_optional(out, "plan_item_id", s.plan_item_id);
    put_optional(out, "reps", s.reps);
    put_optional(out, "weight", s.weight);
    put_optional(out, "rpe", s.rpe);
    return out;
}

json session_to_json(const services::SessionWithSets& sws) {
    const models::WorkoutSession& h = sws.session;
    json out{
        {"id", h.id},
        {"trainee_id", h.trainee_id},
        {"performed_at", h.performed_at},
        {"status", h.status},
        {"plan_id", nullptr},
        {"notes", nullptr},
    };
    put_optional(out, "plan_id", h.plan_id);
    put_optional(out, "notes", h.notes);

    json sets = json::array();
    for (const models::SessionSet& s : sws.sets) {
        sets.push_back(set_to_json(s));
    }
    out["sets"] = std::move(sets);
    return out;
}

}  // namespace

services::SessionInput parse_session_request(const std::string& body) {
    const json obj = parse_object_or_throw(body);

    services::SessionInput in;
    in.plan_id = optional_id(obj, "plan_id");
    in.performed_at = optional_string(obj, "performed_at");
    if (const std::optional<std::string> status = optional_string(obj, "status")) {
        in.status = *status;
    }
    in.notes = optional_string(obj, "notes");

    if (obj.contains("sets")) {
        if (!obj.at("sets").is_array()) {
            invalid("sets must be an array");
        }
        for (const json& entry : obj.at("sets")) {
            in.sets.push_back(parse_set(entry));
        }
    }
    return in;
}

services::SessionPatch parse_session_patch(const std::string& body) {
    const json obj = parse_object_or_throw(body);

    services::SessionPatch patch;
    patch.status = optional_string(obj, "status");
    if (obj.contains("notes")) {
        patch.set_notes = true;
        if (!obj.at("notes").is_null() && !obj.at("notes").is_string()) {
            invalid("notes must be a string or null");
        }
        if (obj.at("notes").is_string() &&
            !obj.at("notes").get<std::string>().empty()) {
            patch.notes = obj.at("notes").get<std::string>();
        }
    }
    return patch;
}

crow::response session_response(int status, const services::SessionWithSets& s) {
    return http::json_response(status, session_to_json(s));
}

crow::response session_list_response(
    const std::vector<services::SessionWithSets>& sessions) {
    json arr = json::array();
    for (const services::SessionWithSets& s : sessions) {
        arr.push_back(session_to_json(s));
    }
    return http::json_response(200, json{{"sessions", std::move(arr)}});
}

}  // namespace fitplan::dto
