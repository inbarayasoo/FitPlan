#include "dto/TraineeDto.hpp"

#include <nlohmann/json.hpp>

#include "http/ApiError.hpp"
#include "http/Json.hpp"

namespace fitplan::dto {

namespace {

using nlohmann::json;

// The public shape of a user: no password_hash, matching GET /api/auth/me.
json user_to_json(const models::User& u) {
    return json{
        {"id", u.id},
        {"email", u.email},
        {"role", u.role},
        {"display_name", u.display_name},
        {"created_at", u.created_at},
    };
}

}  // namespace

AttachTraineeRequest parse_attach_trainee_request(const std::string& body) {
    const json parsed = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (!parsed.is_object() || !parsed.contains("email") ||
        !parsed.at("email").is_string()) {
        throw http::ApiError(http::ApiErrorKind::kInvalidInput,
                             "body must be a JSON object with a string \"email\"");
    }
    std::string email = parsed.at("email").get<std::string>();
    if (email.find_first_not_of(" \t\r\n") == std::string::npos) {
        throw http::ApiError(http::ApiErrorKind::kInvalidInput,
                             "email must not be blank");
    }
    return AttachTraineeRequest{std::move(email)};
}

crow::response trainee_list_response(const std::vector<models::User>& trainees) {
    json arr = json::array();
    for (const models::User& u : trainees) {
        arr.push_back(user_to_json(u));
    }
    return http::json_response(200, json{{"trainees", std::move(arr)}});
}

}  // namespace fitplan::dto
