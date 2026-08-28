#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace fitplan::util {

// The verified facts carried by a valid access token.
struct TokenClaims {
    std::int64_t user_id = 0;
    std::string role;
};

// Signs an HS256 access token for (user_id, role) using `secret`. Issued now,
// expires `ttl_seconds` later. Returns the compact "header.payload.signature"
// string.
std::string make_access_token(std::int64_t user_id, const std::string& role,
                              const std::string& secret, std::int64_t ttl_seconds);

// Checks signature, issuer, and expiry against `secret`. Returns the claims on
// success, or std::nullopt if the token is malformed, tampered, expired, or
// signed with a different secret. Never throws.
std::optional<TokenClaims> verify_access_token(const std::string& token,
                                               const std::string& secret);

}  // namespace fitplan::util