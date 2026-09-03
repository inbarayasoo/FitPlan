#pragma once

#include <cstdint>
#include <string>

namespace fitplan::models {

// Mirrors one row of `email_verification_tokens`: the pending email-confirmation
// code for one user (user_id is UNIQUE, so there is at most one). Only the
// SHA-256 hash of the 6-digit code is stored. The row carries its own expiry and
// a failed-attempt counter so a short numeric code cannot be brute-forced.
struct EmailVerificationToken {
    std::int64_t id = 0;
    std::int64_t user_id = 0;
    std::string code_hash;
    std::string expires_at;  // UTC ISO-8601, e.g. "2026-09-02 14:30:00"
    int attempts = 0;
    std::string issued_at;  // UTC ISO-8601; when this code was (re)issued
};

}  // namespace fitplan::models
