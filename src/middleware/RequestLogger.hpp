#pragma once

#include <crow.h>
#include <spdlog/spdlog.h>

#include <chrono>

namespace fitplan::middleware {

// Crow middleware: one structured log line per request - method, path, status,
// and how long the handler took. A textbook "cross-cutting concern": every route
// wants it, no route should have to write it.
struct RequestLogger {
    struct context {
        std::chrono::steady_clock::time_point started_at;
    };

    void before_handle(crow::request&, crow::response&, context& ctx) {
        ctx.started_at = std::chrono::steady_clock::now();
    }

    void after_handle(crow::request& req, crow::response& res, context& ctx) {
        const auto elapsed = std::chrono::steady_clock::now() - ctx.started_at;
        const auto micros =
            std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
        spdlog::info("{} {} -> {} ({} us)", crow::method_name(req.method), req.url,
                     res.code, micros);
    }
};

}  // namespace fitplan::middleware
