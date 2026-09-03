#include "util/VerificationCode.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <set>
#include <string>

namespace {

using fitplan::util::generate_verification_code;
using fitplan::util::sha256_hex;

bool all_digits(const std::string& s) {
    return std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
}

bool all_lower_hex(const std::string& s) {
    return std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

TEST(VerificationCodeTest, IsAlwaysSixDigits) {
    for (int i = 0; i < 200; ++i) {
        const std::string code = generate_verification_code();
        EXPECT_EQ(code.size(), 6u);
        EXPECT_TRUE(all_digits(code)) << "got: " << code;
    }
}

TEST(VerificationCodeTest, DoesNotReturnTheSameCodeEveryTime) {
    std::set<std::string> seen;
    for (int i = 0; i < 50; ++i) {
        seen.insert(generate_verification_code());
    }
    EXPECT_GT(seen.size(), 1u);
}

TEST(VerificationCodeTest, Sha256HexMatchesKnownVectors) {
    EXPECT_EQ(sha256_hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(sha256_hex("123456"),
              "8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92");
}

TEST(VerificationCodeTest, Sha256HexIsDeterministicLowercaseAndInputSensitive) {
    const std::string a = sha256_hex("042317");
    EXPECT_EQ(a.size(), 64u);
    EXPECT_TRUE(all_lower_hex(a));
    EXPECT_EQ(a, sha256_hex("042317"));
    EXPECT_NE(a, sha256_hex("042318"));
}

}  // namespace
