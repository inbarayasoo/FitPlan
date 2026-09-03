#include "util/VerificationCode.hpp"

#include <sodium.h>

#include <array>
#include <cstdint>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fitplan::util {

namespace {

// libsodium must be initialised once before any other call (see util/Hashing.cpp
// for the same guard - each translation unit keeps its own, all idempotent).
void ensure_sodium_initialised() {
    static std::once_flag init_flag;
    std::call_once(init_flag, [] {
        if (sodium_init() < 0) {
            throw std::runtime_error("libsodium initialisation failed");
        }
    });
}

}  // namespace

std::string generate_verification_code() {
    ensure_sodium_initialised();

    const std::uint32_t value = randombytes_uniform(1000000);  // uniform in [0, 999999]
    std::ostringstream out;
    out << std::setw(6) << std::setfill('0') << value;
    return out.str();
}

std::string sha256_hex(const std::string& input) {
    ensure_sodium_initialised();

    // Copy to unsigned char to hand libsodium the type it wants without a
    // reinterpret_cast (same approach as util/RsaKey.cpp).
    const std::vector<unsigned char> bytes(input.begin(), input.end());
    std::array<unsigned char, crypto_hash_sha256_BYTES> digest{};
    crypto_hash_sha256(digest.data(), bytes.data(), bytes.size());

    std::array<char, crypto_hash_sha256_BYTES * 2 + 1> hex{};
    sodium_bin2hex(hex.data(), hex.size(), digest.data(), digest.size());
    return {hex.data()};
}

}  // namespace fitplan::util
