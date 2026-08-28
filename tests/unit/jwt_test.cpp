#include "util/Jwt.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace {

using fitplan::util::make_access_token;
using fitplan::util::verify_access_token;

constexpr const char* kSecret = "test-secret";
constexpr std::int64_t kOneHour = 3600;

TEST(JwtTest, VerifyReturnsTheClaimsOfAFreshToken) {
    const std::string token = make_access_token(42, "coach", kSecret, kOneHour);

    const auto claims = verify_access_token(token, kSecret);

    ASSERT_TRUE(claims.has_value());
    EXPECT_EQ(claims->user_id, 42);
    EXPECT_EQ(claims->role, "coach");
}

TEST(JwtTest, VerifyRejectsATokenSignedWithAnotherSecret) {
    const std::string token = make_access_token(1, "trainee", kSecret, kOneHour);

    EXPECT_FALSE(verify_access_token(token, "a-different-secret").has_value());
}

TEST(JwtTest, VerifyRejectsATamperedToken) {
    std::string token = make_access_token(1, "trainee", kSecret, kOneHour);

    // Flip the first character of the payload section (right after the first
    // dot). That position holds 6 significant bits, so any change alters the
    // decoded bytes and the signature no longer matches.
    const std::size_t payload_start = token.find('.') + 1;
    token[payload_start] = (token[payload_start] == 'A') ? 'B' : 'A';

    EXPECT_FALSE(verify_access_token(token, kSecret).has_value());
}

TEST(JwtTest, VerifyRejectsAnExpiredToken) {
    const std::string token = make_access_token(1, "trainee", kSecret, -1);

    EXPECT_FALSE(verify_access_token(token, kSecret).has_value());
}

TEST(JwtTest, VerifyRejectsGarbage) {
    EXPECT_FALSE(verify_access_token("not.a.jwt", kSecret).has_value());
    EXPECT_FALSE(verify_access_token("", kSecret).has_value());
}

}  // namespace