#include "util/Hashing.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

using fitplan::util::hash_password;
using fitplan::util::verify_password;

TEST(HashingTest, VerifyAcceptsTheOriginalPassword) {
    const std::string hash = hash_password("correct horse battery staple");

    EXPECT_TRUE(verify_password(hash, "correct horse battery staple"));
}

TEST(HashingTest, VerifyRejectsAWrongPassword) {
    const std::string hash = hash_password("correct horse battery staple");

    EXPECT_FALSE(verify_password(hash, "Correct Horse Battery Staple"));
    EXPECT_FALSE(verify_password(hash, ""));
}

TEST(HashingTest, HashIsSaltedSoTwoHashesOfOnePasswordDiffer) {
    const std::string a = hash_password("same-password");
    const std::string b = hash_password("same-password");

    EXPECT_NE(a, b);
    EXPECT_TRUE(verify_password(a, "same-password"));
    EXPECT_TRUE(verify_password(b, "same-password"));
}

TEST(HashingTest, VerifyReturnsFalseForAGarbageHash) {
    EXPECT_FALSE(verify_password("not-a-valid-hash", "whatever"));
    EXPECT_FALSE(verify_password("", "whatever"));
}

}  // namespace