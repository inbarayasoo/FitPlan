#include "config/Config.hpp"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <exception>
#include <optional>
#include <string>
#include <utility>

namespace fitplan {
namespace {

std::optional<std::string> read_env(const char* key) {
    const char* raw = std::getenv(key);
    if (raw == nullptr || raw[0] == '\0') {
        return std::nullopt;
    }
    return std::string{raw};
}

std::string env_or(const char* key, std::string fallback) {
    if (auto value = read_env(key)) {
        return std::move(*value);
    }
    return fallback;
}

template <typename Int>
Int env_int_or(const char* key, Int fallback) {
    const auto value = read_env(key);
    if (!value) {
        return fallback;
    }
    try {
        return static_cast<Int>(std::stoll(*value));
    } catch (const std::exception&) {
        spdlog::warn("Config: {}='{}' is not a valid integer; using default {}", key, *value,
                     fallback);
        return fallback;
    }
}

}  // namespace

Config Config::from_env() {
    Config cfg;
    cfg.host = env_or("FITPLAN_HOST", cfg.host);
    cfg.port = env_int_or<std::uint16_t>("FITPLAN_PORT", cfg.port);
    cfg.database_path = env_or("FITPLAN_DB_PATH", cfg.database_path);
    cfg.migrations_dir = env_or("FITPLAN_MIGRATIONS_DIR", cfg.migrations_dir);
    cfg.jwt_secret = env_or("FITPLAN_JWT_SECRET", cfg.jwt_secret);
    cfg.jwt_ttl_seconds = env_int_or<std::int64_t>("FITPLAN_JWT_TTL_SECONDS", cfg.jwt_ttl_seconds);
    cfg.log_level = env_or("FITPLAN_LOG_LEVEL", cfg.log_level);
    cfg.thread_count = env_int_or<std::size_t>("FITPLAN_THREADS", cfg.thread_count);
    cfg.web_dir = env_or("FITPLAN_WEB_DIR", cfg.web_dir);
    cfg.docs_dir = env_or("FITPLAN_DOCS_DIR", cfg.docs_dir);
    cfg.google_client_id = env_or("FITPLAN_GOOGLE_CLIENT_ID", cfg.google_client_id);
    return cfg;
}

bool Config::uses_insecure_jwt_secret() const {
    return jwt_secret == "dev-insecure-secret-change-me";
}

bool Config::google_sign_in_enabled() const {
    return !google_client_id.empty();
}

}  // namespace fitplan
