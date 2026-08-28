#include "util/Hashing.hpp"

#include <sodium.h>

#include <mutex>
#include <stdexcept>
#include <string>

namespace fitplan::util {

namespace {

// libsodium must be initialised once before any other call. std::call_once keeps
// that safe even if the first hash happens on two request threads at once.
void ensure_sodium_initialised() {
    static std::once_flag init_flag;
    std::call_once(init_flag, [] {
        if (sodium_init() < 0) {
            throw std::runtime_error("libsodium initialisation failed");
        }
    });
}

}  // namespace

std::string hash_password(const std::string& password) {
    ensure_sodium_initialised();

    char hashed[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(hashed,
                          password.c_str(), password.size(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        throw std::runtime_error("password hashing failed (out of memory?)");
    }
    return std::string(hashed);
}

bool verify_password(const std::string& hash, const std::string& password) {
    ensure_sodium_initialised();

    if (hash.empty()) {
        return false;
    }
    return crypto_pwhash_str_verify(hash.c_str(),
                                    password.c_str(), password.size()) == 0;
}

}  // namespace fitplan::util