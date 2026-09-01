#include "services/AuthService.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "db/Database.hpp"
#include "repositories/UserRepository.hpp"
#include "services/AuthError.hpp"
#include "util/Jwt.hpp"

namespace {

using fitplan::db::Database;
using fitplan::repositories::UserRepository;
using fitplan::services::AuthError;
using fitplan::services::AuthErrorKind;
using fitplan::services::AuthService;
using fitplan::util::verify_access_token;

std::string migrations_dir() {
    return FITPLAN_TEST_MIGRATIONS_DIR;
}

constexpr const char* kSecret = "unit-test-secret";
constexpr std::int64_t kTtl = 3600;

class AuthServiceTest : public ::testing::Test {
protected:
    Database db_{":memory:", migrations_dir()};
    UserRepository users_{db_.connection()};
    AuthService auth_{users_, kSecret, kTtl};
};

TEST_F(AuthServiceTest, RegisterReturnsAUserAndAWorkingToken) {
    const auto out = auth_.register_user("coach@example.com", "password123", "coach", "Coach One");

    EXPECT_GT(out.user.id, 0);
    EXPECT_EQ(out.user.email, "coach@example.com");
    EXPECT_EQ(out.user.role, "coach");

    const auto claims = verify_access_token(out.access_token, kSecret);
    ASSERT_TRUE(claims.has_value());
    EXPECT_EQ(claims->user_id, out.user.id);
    EXPECT_EQ(claims->role, "coach");
}

TEST_F(AuthServiceTest, RegisterStoresAHashNotThePlaintext) {
    const auto out = auth_.register_user("t@example.com", "password123", "trainee", "Trainee");

    EXPECT_NE(out.user.password_hash, "password123");
    EXPECT_FALSE(out.user.password_hash.empty());
}

TEST_F(AuthServiceTest, RegisterRejectsADuplicateEmail) {
    auth_.register_user("dup@example.com", "password123", "coach", "First");

    try {
        auth_.register_user("dup@example.com", "password123", "trainee", "Second");
        FAIL() << "expected AuthError";
    } catch (const AuthError& e) {
        EXPECT_EQ(e.kind(), AuthErrorKind::kEmailAlreadyUsed);
    }
}

TEST_F(AuthServiceTest, RegisterRejectsAnUnknownRole) {
    try {
        auth_.register_user("x@example.com", "password123", "admin", "X");
        FAIL() << "expected AuthError";
    } catch (const AuthError& e) {
        EXPECT_EQ(e.kind(), AuthErrorKind::kInvalidInput);
    }
}

TEST_F(AuthServiceTest, RegisterRejectsAShortPassword) {
    try {
        auth_.register_user("x@example.com", "short", "coach", "X");
        FAIL() << "expected AuthError";
    } catch (const AuthError& e) {
        EXPECT_EQ(e.kind(), AuthErrorKind::kInvalidInput);
    }
}

TEST_F(AuthServiceTest, LoginSucceedsWithCorrectPassword) {
    auth_.register_user("login@example.com", "password123", "coach", "Coach");

    const auto out = auth_.login("login@example.com", "password123");

    EXPECT_EQ(out.user.email, "login@example.com");
    EXPECT_TRUE(verify_access_token(out.access_token, kSecret).has_value());
}

TEST_F(AuthServiceTest, LoginRejectsAWrongPassword) {
    auth_.register_user("login@example.com", "password123", "coach", "Coach");

    try {
        auth_.login("login@example.com", "wrong-password");
        FAIL() << "expected AuthError";
    } catch (const AuthError& e) {
        EXPECT_EQ(e.kind(), AuthErrorKind::kInvalidCredentials);
    }
}

TEST_F(AuthServiceTest, LoginRejectsAnUnknownEmail) {
    try {
        auth_.login("nobody@example.com", "password123");
        FAIL() << "expected AuthError";
    } catch (const AuthError& e) {
        EXPECT_EQ(e.kind(), AuthErrorKind::kInvalidCredentials);
    }
}

TEST_F(AuthServiceTest, AuthenticatedUserReturnsTheStoredUser) {
    const auto out = auth_.register_user("me@example.com", "password123", "trainee", "Me");

    const auto user = auth_.authenticated_user(out.user.id);

    EXPECT_EQ(user.id, out.user.id);
    EXPECT_EQ(user.email, "me@example.com");
}

TEST_F(AuthServiceTest, AuthenticatedUserRejectsAMissingId) {
    try {
        auth_.authenticated_user(9999);
        FAIL() << "expected AuthError";
    } catch (const AuthError& e) {
        EXPECT_EQ(e.kind(), AuthErrorKind::kInvalidCredentials);
    }
}

}  // namespace