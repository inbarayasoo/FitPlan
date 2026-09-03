#include "services/AuthService.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <regex>
#include <string>
#include <vector>

#include "db/Database.hpp"
#include "GoogleTokenMint.hpp"
#include "repositories/EmailVerificationTokenRepository.hpp"
#include "repositories/UserRepository.hpp"
#include "RsaTestKey.hpp"
#include "services/AuthError.hpp"
#include "services/EmailSender.hpp"
#include "services/EmailVerificationError.hpp"
#include "services/EmailVerificationService.hpp"
#include "services/GoogleIdTokenVerifier.hpp"
#include "services/GoogleJwks.hpp"
#include "util/Clock.hpp"
#include "util/Jwt.hpp"

namespace {

using fitplan::db::Database;
using fitplan::repositories::EmailVerificationTokenRepository;
using fitplan::repositories::UserRepository;
using fitplan::services::AuthError;
using fitplan::services::AuthErrorKind;
using fitplan::services::AuthOutcome;
using fitplan::services::AuthService;
using fitplan::services::EmailMessage;
using fitplan::services::EmailSender;
using fitplan::services::EmailVerificationError;
using fitplan::services::EmailVerificationErrorKind;
using fitplan::services::EmailVerificationService;
using fitplan::services::GoogleIdTokenVerifier;
using fitplan::services::GoogleJwksCache;
using fitplan::services::JwksDocument;
using fitplan::testutil::GoogleTokenOptions;
using fitplan::testutil::kTestGoogleClientId;
using fitplan::testutil::make_rsa_test_key;
using fitplan::testutil::mint_google_id_token;
using fitplan::testutil::RsaTestKey;
using fitplan::util::verify_access_token;

std::string migrations_dir() {
    return FITPLAN_TEST_MIGRATIONS_DIR;
}

constexpr const char* kSecret = "unit-test-secret";
constexpr std::int64_t kTtl = 3600;

class AuthServiceTest : public ::testing::Test {
protected:
    std::vector<EmailMessage> mail_;
    EmailSender sender_ = [this](const EmailMessage& m) { mail_.push_back(m); };

    Database db_{":memory:", migrations_dir()};
    UserRepository users_{db_.connection()};
    EmailVerificationTokenRepository ev_tokens_{db_.connection()};
    EmailVerificationService ev_{users_, ev_tokens_, sender_, fitplan::util::iso_utc_now,
                                 "http://test"};
    AuthService auth_{users_, kSecret, kTtl, nullptr, &ev_};

    std::string last_code() const {
        std::smatch m;
        return std::regex_search(mail_.back().body, m, std::regex(R"(\d{6})")) ? m.str(0)
                                                                               : std::string{};
    }

    AuthOutcome register_and_verify(const std::string& email, const std::string& password,
                                    const std::string& role, const std::string& name) {
        auth_.register_user(email, password, role, name);
        return auth_.verify_email(email, last_code());
    }
};

TEST_F(AuthServiceTest, RegisterCreatesAnUnverifiedUserAndEmailsACodeButNoToken) {
    const auto out = auth_.register_user("coach@example.com", "password123", "coach", "Coach One");

    EXPECT_GT(out.user.id, 0);
    EXPECT_EQ(out.user.email, "coach@example.com");
    EXPECT_EQ(out.user.role, "coach");
    EXPECT_TRUE(out.verification_required);
    EXPECT_FALSE(out.user.email_verified);
    ASSERT_EQ(mail_.size(), 1u);
    EXPECT_EQ(mail_.front().to_email, "coach@example.com");
}

TEST_F(AuthServiceTest, VerifyEmailIssuesTheFirstTokenAndMarksTheAccount) {
    auth_.register_user("coach@example.com", "password123", "coach", "Coach One");

    const auto out = auth_.verify_email("coach@example.com", last_code());

    EXPECT_TRUE(out.user.email_verified);
    const auto claims = verify_access_token(out.access_token, kSecret);
    ASSERT_TRUE(claims.has_value());
    EXPECT_EQ(claims->user_id, out.user.id);
    EXPECT_EQ(claims->role, "coach");
}

TEST_F(AuthServiceTest, VerifyEmailRejectsAWrongCode) {
    auth_.register_user("coach@example.com", "password123", "coach", "Coach One");
    const std::string wrong = last_code() == "000000" ? "111111" : "000000";

    try {
        auth_.verify_email("coach@example.com", wrong);
        FAIL() << "expected EmailVerificationError";
    } catch (const EmailVerificationError& e) {
        EXPECT_EQ(e.kind(), EmailVerificationErrorKind::kCodeMismatch);
    }
}

TEST_F(AuthServiceTest, LoginIsBlockedUntilTheEmailIsVerified) {
    auth_.register_user("coach@example.com", "password123", "coach", "Coach One");

    try {
        auth_.login("coach@example.com", "password123");
        FAIL() << "expected AuthError";
    } catch (const AuthError& e) {
        EXPECT_EQ(e.kind(), AuthErrorKind::kEmailNotVerified);
    }

    auth_.verify_email("coach@example.com", last_code());
    EXPECT_TRUE(
        verify_access_token(auth_.login("coach@example.com", "password123").access_token, kSecret)
            .has_value());
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
    register_and_verify("login@example.com", "password123", "coach", "Coach");

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

TEST_F(AuthServiceTest, GoogleLoginIsRefusedWhenNoVerifierIsConfigured) {
    // auth_ was built without a GoogleIdTokenVerifier.
    try {
        auth_.login_with_google("any.token.here");
        FAIL() << "expected AuthError";
    } catch (const AuthError& e) {
        EXPECT_EQ(e.kind(), AuthErrorKind::kInvalidInput);
    }
}

// --- Google sign-in ---------------------------------------------------------

class AuthServiceGoogleTest : public ::testing::Test {
protected:
    Database db_{":memory:", migrations_dir()};
    UserRepository users_{db_.connection()};
    RsaTestKey key_ = make_rsa_test_key("google-test-kid");
    GoogleJwksCache jwks_{[this] {
        JwksDocument doc;
        doc.body = RsaTestKey::jwks_document({key_});
        return doc;
    }};
    GoogleIdTokenVerifier verifier_{kTestGoogleClientId, jwks_};
    AuthService auth_{users_, kSecret, kTtl, &verifier_};
};

TEST_F(AuthServiceGoogleTest, FirstSignInWithoutARoleAsksForOneAndCreatesNothing) {
    const auto result = auth_.login_with_google(mint_google_id_token(key_));

    EXPECT_TRUE(result.needs_role);
    EXPECT_TRUE(result.outcome.access_token.empty());
    EXPECT_FALSE(users_.find_by_email("gina@example.com").has_value());
}

TEST_F(AuthServiceGoogleTest, FirstSignInWithARoleCreatesThatAccountWithNoPassword) {
    const auto result = auth_.login_with_google(mint_google_id_token(key_), "coach");

    EXPECT_FALSE(result.needs_role);
    EXPECT_GT(result.outcome.user.id, 0);
    EXPECT_EQ(result.outcome.user.email, "gina@example.com");
    EXPECT_EQ(result.outcome.user.role, "coach");
    EXPECT_EQ(result.outcome.user.auth_provider, "google");
    EXPECT_EQ(result.outcome.user.display_name, "Gina G");
    EXPECT_TRUE(result.outcome.user.password_hash.empty());

    const auto claims = verify_access_token(result.outcome.access_token, kSecret);
    ASSERT_TRUE(claims.has_value());
    EXPECT_EQ(claims->role, "coach");
}

TEST_F(AuthServiceGoogleTest, FirstSignInWithAnUnknownRoleIsRejected) {
    try {
        auth_.login_with_google(mint_google_id_token(key_), "wizard");
        FAIL() << "expected AuthError";
    } catch (const AuthError& e) {
        EXPECT_EQ(e.kind(), AuthErrorKind::kInvalidInput);
    }
}

TEST_F(AuthServiceGoogleTest, SecondSignInIgnoresTheRoleAndReturnsTheSameAccount) {
    const auto first = auth_.login_with_google(mint_google_id_token(key_), "trainee");
    ASSERT_FALSE(first.needs_role);

    const auto second = auth_.login_with_google(mint_google_id_token(key_), "coach");

    EXPECT_FALSE(second.needs_role);
    EXPECT_EQ(second.outcome.user.id, first.outcome.user.id);
    EXPECT_EQ(second.outcome.user.role, "trainee");  // the role from the retry is ignored
}

TEST_F(AuthServiceGoogleTest, LinksToAnExistingLocalAccountByVerifiedEmailKeepingItsRole) {
    const auto local =
        auth_.register_user("coach@example.com", "password123", "coach", "Coach One");

    GoogleTokenOptions opt;
    opt.email = "coach@example.com";
    opt.subject = "google-sub-for-coach";
    // A role in the request is ignored on the link path.
    const auto result = auth_.login_with_google(mint_google_id_token(key_, opt), "trainee");

    EXPECT_FALSE(result.needs_role);
    EXPECT_EQ(result.outcome.user.id, local.user.id);
    EXPECT_EQ(result.outcome.user.role, "coach");           // unchanged
    EXPECT_EQ(result.outcome.user.auth_provider, "local");  // records how the account began
    EXPECT_EQ(result.outcome.user.google_sub, "google-sub-for-coach");

    const auto again = auth_.login_with_google(mint_google_id_token(key_, opt));
    EXPECT_EQ(again.outcome.user.id, local.user.id);
}

TEST_F(AuthServiceGoogleTest, RefusesToLinkWhenTheGoogleEmailIsNotVerified) {
    auth_.register_user("coach@example.com", "password123", "coach", "Coach One");

    GoogleTokenOptions opt;
    opt.email = "coach@example.com";
    opt.email_verified = false;

    try {
        auth_.login_with_google(mint_google_id_token(key_, opt), "trainee");
        FAIL() << "expected AuthError";
    } catch (const AuthError& e) {
        EXPECT_EQ(e.kind(), AuthErrorKind::kEmailAlreadyUsed);
    }
}

TEST_F(AuthServiceGoogleTest, RejectsATokenThatFailsVerification) {
    try {
        auth_.login_with_google("not-a-real-token");
        FAIL() << "expected AuthError";
    } catch (const AuthError& e) {
        EXPECT_EQ(e.kind(), AuthErrorKind::kInvalidCredentials);
    }
}

TEST_F(AuthServiceGoogleTest, FallsBackToTheEmailWhenGoogleSendsNoName) {
    GoogleTokenOptions opt;
    opt.name = "";
    const auto result = auth_.login_with_google(mint_google_id_token(key_, opt), "trainee");

    EXPECT_EQ(result.outcome.user.display_name, "gina@example.com");
}

}  // namespace