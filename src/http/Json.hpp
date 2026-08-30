#pragma once

#include <crow.h>

#include <nlohmann/json.hpp>

namespace fitplan::http {

// Builds a normal "application/json" response from an nlohmann::json value.
// The error-path counterpart is problem_response() in Problem.hpp.
inline crow::response json_response(int status, const nlohmann::json& body) {
    crow::response res(status);
    res.body = body.dump();
    res.set_header("Content-Type", "application/json");
    return res;
}

}  // namespace fitplan::http
