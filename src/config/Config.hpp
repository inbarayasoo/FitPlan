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
//   FITPLAN_WEB_DIR          static frontend directory (default web)
//   FITPLAN_DOCS_DIR         API docs directory        (default docs)
//   FITPLAN_GOOGLE_CLIENT_ID Google OAuth client id for Sign-In (default: empty)
//   FITPLAN_BREVO_API_KEY    Brevo transactional-email API key (default: empty)
//   FITPLAN_EMAIL_FROM       From: address for outbound mail   (default no-reply@fitplan.dev)
//   FITPLAN_EMAIL_FROM_NAME  From: display name                (default FitPlan)
//   FITPLAN_PUBLIC_BASE_URL  base URL shown in emails          (default http://localhost:8080)
struct Config {
    std::string host = "0.0.0.0";
    std::uint16_t port = 8080;
    std::string database_path = "fitplan.db";
    std::string migrations_dir = "src/db/migrations";
    std::string jwt_secret = "dev-insecure-secret-change-me";
    std::int64_t jwt_ttl_seconds = 24LL * 60 * 60;
    std::string log_level = "info";
    std::size_t thread_count = 0;
    std::string web_dir = "web";
    std::string docs_dir = "docs";
    std::string google_client_id;
    std::string brevo_api_key;
    std::string email_from = "no-reply@fitplan.dev";
    std::string email_from_name = "FitPlan";
    std::string public_base_url = "http://localhost:8080";

    // Builds a Config from the current environment. Never throws: malformed
    // values fall back to the default and are reported through spdlog::warn.
    static Config from_env();

    // True when the JWT secret is still the built-in development placeholder.
    [[nodiscard]] bool uses_insecure_jwt_secret() const;

    // True when a Google OAuth client id is configured. When false, the server
    // does not register POST /api/auth/google and the frontend hides the button.
    [[nodiscard]] bool google_sign_in_enabled() const;

    // True when a Brevo API key is configured, so verification emails are really
    // sent. When false the server still requires verification but writes the code
    // to the log instead of emailing it (development / CI).
    [[nodiscard]] bool transactional_email_enabled() const;
};

}  // namespace fitplan
