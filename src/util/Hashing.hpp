#pragma once

#include <string>

namespace fitplan::util {

// Hashes a plaintext password with Argon2id (via libsodium's crypto_pwhash_str).
// The returned string is self-describing: it embeds the algorithm, its cost
// parameters, the random salt, and the derived hash, so verify_password() needs
// nothing else. Throws std::runtime_error if libsodium cannot initialise or the
// hash computation fails (e.g. out of memory).
std::string hash_password(const std::string& password);

// Returns true iff `password` matches the stored `hash`. Returns false for a
// wrong password or a malformed/empty hash. Never throws.
bool verify_password(const std::string& hash, const std::string& password);

}  // namespace fitplan::util