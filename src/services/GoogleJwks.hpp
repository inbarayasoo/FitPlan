#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace fitplan::services {

// Google's JWKS document plus how long it may be cached. `ttl` comes from the
// response's `Cache-Control: max-age`, defaulting to one hour.
struct JwksDocument {
    std::string body;  // the raw JSON
    std::chrono::seconds ttl{std::chrono::hours{1}};
};

// "Go and get Google's JWKS document." The live server does an HTTPS GET; unit
// and integration tests inject a lambda that returns a canned document, so
// nothing below the verifier ever touches the network in CI.
using JwksFetcher = std::function<JwksDocument()>;

// The live fetcher: GET https://www.googleapis.com/oauth2/v3/certs via cpr.
// Throws std::runtime_error on a transport error or a non-200 response.
JwksDocument fetch_google_jwks();

// Parses a JWKS JSON document into a map of key id -> RSA public key (PEM).
// Skips any entry that is not an RSA signing key. Throws std::runtime_error if
// the document is not a JSON object with a "keys" array.
std::unordered_map<std::string, std::string> parse_google_jwks(const std::string& json);

// A thread-safe cache of Google's signing keys. `public_key_pem(kid)` returns
// the PEM for that key id, fetching Google's document on first use, when the
// cached copy has outlived its TTL, or when `kid` is unknown (a key rotation) -
// the last case throttled by `min_refetch_gap` so a stream of bogus-kid tokens
// cannot hammer Google. If a refresh fails but a previous one succeeded, the
// stale keys keep being served.
class GoogleJwksCache {
public:
    explicit GoogleJwksCache(JwksFetcher fetcher = fetch_google_jwks,
                             std::chrono::seconds min_refetch_gap = std::chrono::seconds{60});

    std::optional<std::string> public_key_pem(const std::string& kid);

private:
    void refresh_locked();

    JwksFetcher fetcher_;
    std::chrono::seconds min_refetch_gap_;

    std::mutex mutex_;
    std::unordered_map<std::string, std::string> keys_by_kid_;
    std::chrono::seconds ttl_{std::chrono::hours{1}};
    std::chrono::steady_clock::time_point fetched_at_;
    std::chrono::steady_clock::time_point last_attempt_at_;
    bool ever_fetched_ = false;
};

}  // namespace fitplan::services
