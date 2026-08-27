#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace fitplan {

// Runtime configuration for the server, sourced from environment variables with
// safe defaults so the binary runs out of the box in development.
//
// Recognized variables:
//   FITPLAN_HOST              bind address              (default 0.0.0.0)
//   FITPLAN_PORT              TCP port                  (default 8080)
//   FITPLAN_DB_PATH          SQLite file path          (default fitplan.db)
//   FITPLAN_MIGRATIONS_DIR   dir of *.sql migrations   (default src/db/migrations)
//   FITPLAN_JWT_SECRET       HMAC signing secret       (default: dev-only value)
//   FITPLAN_JWT_TTL_SECONDS  access-token lifetime     (default 86400)
//   FITPLAN_LOG_LEVEL        trace|debug|info|warn|error|off  (default info)
//   FITPLAN_THREADS          worker threads, 0 = auto  (default 0)
struct Config {
    std::string host = "0.0.0.0";
    std::uint16_t port = 8080;
    std::string database_path = "fitplan.db";
    std::string migrations_dir = "src/db/migrations";
    std::string jwt_secret = "dev-insecure-secret-change-me";
    std::int64_t jwt_ttl_seconds = 24 * 60 * 60;
    std::string log_level = "info";
    std::size_t thread_count = 0;

    // Builds a Config from the current environment. Never throws: malformed
    // values fall back to the default and are reported through spdlog::warn.
    static Config from_env();

    // True when the JWT secret is still the built-in development placeholder.
    [[nodiscard]] bool uses_insecure_jwt_secret() const;
};

}  // namespace fitplan
