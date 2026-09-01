#include "util/Hashing.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

using fitplan::util::hash_password;
using fitplan::util::verify_password;

constexpr const char* kPassword = "correct horse battery staple";

// A hash whose Argon2id string has been cut short must not verify, and must not
// crash the parser inside libsodium.
TEST(HashingHardeningTest, RejectsATruncatedHash) {
    const std::string full = hash_password(kPassword);
    const std::string truncated = full.substr(0, full.size() - 8);

    EXPECT_FALSE(verify_password(truncated, kPassword));
}

// Extra bytes glued onto an otherwise valid hash string invalidate it: the
// encoded form must match exactly, end to end.
TEST(HashingHardeningTest, RejectsAHashWithTrailingGarbage) {
    const std::string tampered = hash_password(kPassword) + "AAAA";

    EXPECT_FALSE(verify_password(tampered, kPassword));
}

// One flipped character anywhere in the encoded salt or digest breaks the match,
// even with the correct password.
TEST(HashingHardeningTest, RejectsAHashWithACorruptedInteriorCharacter) {
    std::string hash = hash_password(kPassword);
    const std::size_t i = hash.size() - 5;
    hash[i] = (hash[i] == 'A') ? 'B' : 'A';

    EXPECT_FALSE(verify_password(hash, kPassword));
}

// A well-formed hash string from a different algorithm (here a bcrypt string) is
// not something we ever produced, so it can never verify.
TEST(HashingHardeningTest, RejectsAHashFromADifferentAlgorithm) {
    const std::string bcrypt = "$2b$12$C6UzMDM.H6dfI/f/IKcEeO.6aVE1NA5.rXhVo1sBc.6JQpqR0R2Iy";

    EXPECT_FALSE(verify_password(bcrypt, kPassword));
}

// Leading or trailing whitespace is not trimmed away - the stored value has to
// be the bare hash.
TEST(HashingHardeningTest, RejectsAWhitespacePaddedHash) {
    const std::string padded = "  " + hash_password(kPassword) + "\n";

    EXPECT_FALSE(verify_password(padded, kPassword));
}

// Whatever lands in the hash column - random text, control bytes, an embedded
// NUL, a very long string - verify_password returns false and never throws.
TEST(HashingHardeningTest, NeverThrowsOnArbitraryInput) {
    const std::vector<std::string> nasty = {
        "not-a-hash",
        "$argon2id$",
        "$argon2id$v=19$m=65536,t=2,p=1$",
        std::string("\x01\x02\x03 binary \x7f", 12),
        std::string("embedded\0nul", 12),
        std::string(4096, 'x'),
    };

    for (const std::string& value : nasty) {
        EXPECT_NO_THROW({ EXPECT_FALSE(verify_password(value, kPassword)); });
    }
}

}  // namespace
