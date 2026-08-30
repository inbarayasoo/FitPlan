#include "dto/PlanDto.hpp"

#include <nlohmann/json.hpp>

#include <optional>

#include "http/ApiError.hpp"
#include "http/Json.hpp"
#include "models/PlanItem.hpp"
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

std::int64_t required_int(const json& obj, const char* key) {
    if (!obj.contains(key) || !obj.at(key).is_number_integer()) {
        invalid(std::string("missing or non-integer field: ") + key);
    }
    return obj.at(key).get<std::int64_t>();
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

services::PlanItemInput parse_item(const json& obj) {
    if (!obj.is_object()) {
        invalid("each entry of items must be a JSON object");
    }
    services::PlanItemInput item;
    item.exercise_id = required_int(obj, "exercise_id");
    item.day_label = optional_string(obj, "day_label");
    item.target_sets = optional_int(obj, "target_sets");
    item.target_reps = optional_int(obj, "target_reps");
    item.target_weight = optional_double(obj, "target_weight");
    item.rest_seconds = optional_int(obj, "rest_seconds");
    item.notes = optional_string(obj, "notes");
    item.video_url = optional_string(obj, "video_url");

    if (item.video_url && !util::is_allowed_video_url(*item.video_url)) {
        invalid("item video_url must be an https youtube.com / youtu.be / instagram.com link");
    }
    return item;
}

// --- serialization ----------------------------------------------------------

template <class T>
void put_optional(json& out, const char* key, const std::optional<T>& v) {
    if (v.has_value()) {
        out[key] = *v;
    } else {
        out[key] = nullptr;
    }
}

json item_to_json(const models::PlanItem& it) {
    json out{
        {"id", it.id},
        {"plan_id", it.plan_id},
        {"exercise_id", it.exercise_id},
        {"order_index", it.order_index},
        {"video_embed_url", nullptr},
    };
    put_optional(out, "day_label", it.day_label);
    put_optional(out, "target_sets", it.target_sets);
    put_optional(out, "target_reps", it.target_reps);
    put_optional(out, "target_weight", it.target_weight);
    put_optional(out, "rest_seconds", it.rest_seconds);
    put_optional(out, "notes", it.notes);
    put_optional(out, "video_url", it.video_url);
    if (it.video_url) {
        if (const auto embed = util::youtube_embed_url(*it.video_url)) {
            out["video_embed_url"] = *embed;
        }
    }
    return out;
}

json header_to_json(const models::WorkoutPlan& p) {
    json out{
        {"id", p.id},
        {"coach_id", p.coach_id},
        {"trainee_id", p.trainee_id},
        {"name", p.name},
        {"is_active", p.is_active},
        {"created_at", p.created_at},
        {"notes", nullptr},
    };
    put_optional(out, "notes", p.notes);
    return out;
}

}  // namespace

services::PlanInput parse_plan_request(const std::string& body) {
    const json obj = parse_object_or_throw(body);

    services::PlanInput input;
    input.trainee_id = required_int(obj, "trainee_id");
    input.name = required_string(obj, "name");
    input.notes = optional_string(obj, "notes");

    if (!obj.contains("items") || !obj.at("items").is_array() ||
        obj.at("items").empty()) {
        invalid("items must be a non-empty array");
    }
    for (const json& entry : obj.at("items")) {
        input.items.push_back(parse_item(entry));
    }
    return input;
}

crow::response plan_response(int status, const services::PlanWithItems& plan) {
    json body = header_to_json(plan.plan);
    json items = json::array();
    for (const models::PlanItem& it : plan.items) {
        items.push_back(item_to_json(it));
    }
    body["items"] = std::move(items);
    return http::json_response(status, body);
}

crow::response plan_list_response(const std::vector<models::WorkoutPlan>& plans) {
    json arr = json::array();
    for (const models::WorkoutPlan& p : plans) {
        arr.push_back(header_to_json(p));
    }
    return http::json_response(200, json{{"plans", std::move(arr)}});
}

}  // namespace fitplan::dto
