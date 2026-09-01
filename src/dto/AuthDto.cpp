#include "dto/AuthDto.hpp"

#include <nlohmann/json.hpp>

#include "services/AuthError.hpp"

namespace fitplan::dto {

namespace {

using nlohmann::json;

json parse_object_or_throw(const std::string& body) {
    // allow_exceptions = false: json::parse returns a "discarded" value instead
    // of throwing, so we control the error message.
    const json parsed = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (!parsed.is_object()) {
        throw services::AuthError(services::AuthErrorKind::kInvalidInput,
                                  "request body must be a JSON object");
    }
    return parsed;
}

std::string required_string(const json& obj, const char* key) {
    if (!obj.contains(key) || !obj.at(key).is_string()) {
        throw services::AuthError(services::AuthErrorKind::kInvalidInput,
                                  std::string("missing or non-string field: ") + key);
    }
    return obj.at(key).get<std::string>();
}

json user_to_json(const models::User& u) {
    return json{
        {"id", u.id},
        {"email", u.email},
        {"role", u.role},
        {"display_name", u.display_name},
        {"created_at", u.created_at},
    };
}

crow::response json_response(int status, const json& body) {
    crow::response res(status);
    res.body = body.dump();
    res.set_header("Content-Type", "application/json");
    return res;
}

}  // namespace

RegisterRequest parse_register_request(const std::string& body) {
    const json obj = parse_object_or_throw(body);
    return RegisterRequest{
        required_string(obj, "email"),
        required_string(obj, "password"),
        required_string(obj, "role"),
        required_string(obj, "display_name"),
    };
}

LoginRequest parse_login_request(const std::string& body) {
    const json obj = parse_object_or_throw(body);
    return LoginRequest{
        required_string(obj, "email"),
        required_string(obj, "password"),
    };
}

crow::response auth_response(int status, const services::AuthOutcome& outcome) {
    return json_response(status, json{
                                     {"access_token", outcome.access_token},
                                     {"user", user_to_json(outcome.user)},
                                 });
}

crow::response user_response(int status, const models::User& user) {
    return json_response(status, user_to_json(user));
}

}  // namespace fitplan::dto
