#include "services/GoogleJwks.hpp"

#include <cpr/cpr.h>
#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>

#include <cctype>
#include <exception>
#include <string>
#include <utility>

#include "util/RsaKey.hpp"

namespace fitplan::services {

namespace {

using nlohmann::json;

constexpr const char* kGoogleJwksUrl = "https://www.googleapis.com/oauth2/v3/certs";

// Pulls the number out of a "Cache-Control: ..., max-age=NNNN, ..." header.
std::optional<long> max_age_seconds(const std::string& cache_control) {
    const auto pos = cache_control.find("max-age=");
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    auto digits = pos + std::string("max-age=").size();
    long value = 0;
    bool any = false;
    while (digits < cache_control.size() &&
           std::isdigit(static_cast<unsigned char>(cache_control[digits])) != 0) {
        value = value * 10 + (cache_control[digits] - '0');
        ++digits;
        any = true;
    }
    return any ? std::optional<long>{value} : std::nullopt;
}

}  // namespace

JwksDocument fetch_google_jwks() {
    const cpr::Response res =
        cpr::Get(cpr::Url{kGoogleJwksUrl}, cpr::Timeout{std::chrono::seconds{5}});

    if (res.error) {
        throw std::runtime_error("Google JWKS fetch failed: " + res.error.message);
    }
    if (res.status_code != 200) {
        throw std::runtime_error("Google JWKS fetch returned HTTP " +
                                 std::to_string(res.status_code));
    }

    JwksDocument doc;
    doc.body = res.text;

    // cpr's header map compares keys case-insensitively.
    const auto header = res.header.find("cache-control");
    if (header != res.header.end()) {
        if (const auto age = max_age_seconds(header->second); age && *age > 0) {
            doc.ttl = std::chrono::seconds{*age};
        }
    }
    return doc;
}

std::unordered_map<std::string, std::string> parse_google_jwks(const std::string& json_text) {
    const json parsed = json::parse(json_text, nullptr, /*allow_exceptions=*/false);
    if (!parsed.is_object() || !parsed.contains("keys") || !parsed.at("keys").is_array()) {
        throw std::runtime_error("JWKS document is not an object with a \"keys\" array");
    }

    std::unordered_map<std::string, std::string> keys;
    for (const json& key : parsed.at("keys")) {
        if (!key.is_object()) {
            continue;
        }
        const bool is_rsa = key.value("kty", "") == "RSA";
        const bool for_signing = key.value("use", "sig") == "sig";
        if (!is_rsa || !for_signing) {
            continue;
        }
        if (!key.contains("kid") || !key.contains("n") || !key.contains("e")) {
            continue;
        }
        keys.emplace(key.at("kid").get<std::string>(),
                     util::rsa_public_key_pem_from_jwk(key.at("n").get<std::string>(),
                                                       key.at("e").get<std::string>()));
    }
    return keys;
}

GoogleJwksCache::GoogleJwksCache(JwksFetcher fetcher, std::chrono::seconds min_refetch_gap)
    : fetcher_(std::move(fetcher)), min_refetch_gap_(min_refetch_gap) {}

std::optional<std::string> GoogleJwksCache::public_key_pem(const std::string& kid) {
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::chrono::steady_clock::now();

    if (!ever_fetched_ || now - fetched_at_ >= ttl_) {
        refresh_locked();
    }

    if (const auto found = keys_by_kid_.find(kid); found != keys_by_kid_.end()) {
        return found->second;
    }

    // Unknown key id: Google may have rotated its keys. Refetch, but not more
    // often than min_refetch_gap_.
    if (now - last_attempt_at_ >= min_refetch_gap_) {
        refresh_locked();
        if (const auto found = keys_by_kid_.find(kid); found != keys_by_kid_.end()) {
            return found->second;
        }
    }
    return std::nullopt;
}

void GoogleJwksCache::refresh_locked() {
    last_attempt_at_ = std::chrono::steady_clock::now();
    try {
        const JwksDocument doc = fetcher_();
        keys_by_kid_ = parse_google_jwks(doc.body);
        ttl_ = doc.ttl;
        fetched_at_ = std::chrono::steady_clock::now();
        ever_fetched_ = true;
    } catch (const std::exception& ex) {
        if (!ever_fetched_) {
            throw;  // nothing cached to fall back on
        }
        spdlog::warn("GoogleJwksCache: refresh failed ({}); serving cached keys", ex.what());
    }
}

}  // namespace fitplan::services
