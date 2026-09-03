#pragma once

#include <string>

namespace fitplan::util {

// A fresh six-digit numeric code as a zero-padded string ("042317", and
// "000123" is a valid result). Drawn uniformly from [0, 999999] with
// libsodium's randombytes_uniform - no modulo bias, cryptographically random.
std::string generate_verification_code();

// Lowercase hex SHA-256 of `input` (64 characters). Used to store only the hash
// of a verification code, never the code itself.
std::string sha256_hex(const std::string& input);

}  // namespace fitplan::util
