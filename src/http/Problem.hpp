#pragma once

#include <crow.h>

#include <nlohmann/json.hpp>

#include <string>

namespace fitplan::http {

// Builds an "application/problem+json" error response (the shape from RFC 7807):
// a small JSON object with a human title, the numeric status, and a detail line.
// Every error the API returns goes through here, so clients see one format.
inline crow::response problem_response(int status, const std::string& title,
                                       const std::string& detail) {
    const nlohmann::json body{
        {"type", "about:blank"},
        {"title", title},
        {"status", status},
        {"detail", detail},
    };
    crow::response res(status);
    res.body = body.dump();
    res.set_header("Content-Type", "application/problem+json");
    return res;
}

}  // namespace fitplan::http
