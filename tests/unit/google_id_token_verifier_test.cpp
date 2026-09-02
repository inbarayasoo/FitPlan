#include "services/GoogleIdTokenVerifier.hpp"

#include <gtest/gtest.h>
#include <jwt-cpp/jwt.h>

#include <chrono>
#include <string>

#include "GoogleTokenMint.hpp"
#include "RsaTestKey.hpp"
#include "services/GoogleJwks.hpp"

namespace {

using fitplan::services::GoogleIdTokenVerifier;
using fitplan::services::GoogleJwksCache;
using fitplan::services::JwksDocument;
using fitplan::testutil::GoogleTokenOptions;
using fitplan::testutil::kTestGoogleClientId;
using fitplan::testutil::make_rsa_test_key;
using fitplan::testutil::mint_google_id_token;
using fitplan::testutil::RsaTestKey;

// A verifier whose JWKS cache publishes exactly one key: key_.
class GoogleIdTokenVerifierTest : public ::testing::Test {
protected:
    RsaTestKey key_ = make_rsa_test_key("test-kid-1");
    GoogleJwksCache cache_{[this] {
        JwksDocument doc;
        doc.body = RsaTestKey::jwks_document({key_});
        return doc;
    }};
    GoogleIdTokenVerifier verifier_{kTestGoogleClientId, cache_};
};

TEST_F(GoogleIdTokenVerifierTest, AcceptsAWellFormedToken) {
    const auto identity = verifier_.verify(mint_google_id_token(key_));

    ASSERT_TRUE(identity.has_value());
    EXPECT_EQ(identity->subject, "google-sub-42");
    EXPECT_EQ(identity->email, "gina@example.com");
    EXPECT_TRUE(identity->email_verified);
    EXPECT_EQ(identity->name, "Gina G");
}

TEST_F(GoogleIdTokenVerifierTest, RejectsAnExpiredToken) {
    GoogleTokenOptions opt;
    opt.lifetime = -std::chrono::minutes{5};
    EXPECT_FALSE(verifier_.verify(mint_google_id_token(key_, opt)).has_value());
}

TEST_F(GoogleIdTokenVerifierTest, RejectsAWrongAudience) {
    GoogleTokenOptions opt;
    opt.audience = "someone-elses-app.apps.googleusercontent.com";
    EXPECT_FALSE(verifier_.verify(mint_google_id_token(key_, opt)).has_value());
}

TEST_F(GoogleIdTokenVerifierTest, RejectsAWrongIssuer) {
    GoogleTokenOptions opt;
    opt.issuer = "https://evil.example.com";
    EXPECT_FALSE(verifier_.verify(mint_google_id_token(key_, opt)).has_value());
}

TEST_F(GoogleIdTokenVerifierTest, AcceptsTheBareAccountsGoogleComIssuer) {
    GoogleTokenOptions opt;
    opt.issuer = "accounts.google.com";
    EXPECT_TRUE(verifier_.verify(mint_google_id_token(key_, opt)).has_value());
}

TEST_F(GoogleIdTokenVerifierTest, RejectsATokenSignedByAKeyGoogleDoesNotPublish) {
    const RsaTestKey attacker = make_rsa_test_key("test-kid-1");  // same kid, different key
    EXPECT_FALSE(verifier_.verify(mint_google_id_token(attacker)).has_value());
}

TEST_F(GoogleIdTokenVerifierTest, RejectsAnUnknownKeyId) {
    GoogleTokenOptions opt;
    opt.key_id = "a-kid-not-in-the-jwks";
    EXPECT_FALSE(verifier_.verify(mint_google_id_token(key_, opt)).has_value());
}

TEST_F(GoogleIdTokenVerifierTest, RejectsAStructurallyBrokenToken) {
    EXPECT_FALSE(verifier_.verify("not.a.jwt").has_value());
    EXPECT_FALSE(verifier_.verify("").has_value());
}

TEST_F(GoogleIdTokenVerifierTest, RejectsATamperedPayload) {
    std::string token = mint_google_id_token(key_);
    const auto first_dot = token.find('.');
    ASSERT_NE(first_dot, std::string::npos);
    char& c = token[first_dot + 5];  // a byte inside the payload segment
    c = (c == 'A') ? 'B' : 'A';
    EXPECT_FALSE(verifier_.verify(token).has_value());
}

TEST_F(GoogleIdTokenVerifierTest, ReportsEmailVerifiedFalseWhenGoogleSaysSo) {
    GoogleTokenOptions opt;
    opt.email_verified = false;

    const auto identity = verifier_.verify(mint_google_id_token(key_, opt));

    ASSERT_TRUE(identity.has_value());
    EXPECT_FALSE(identity->email_verified);
}

TEST_F(GoogleIdTokenVerifierTest, TreatsAMissingEmailVerifiedClaimAsNotVerified) {
    GoogleTokenOptions opt;
    opt.include_email_verified = false;

    const auto identity = verifier_.verify(mint_google_id_token(key_, opt));

    ASSERT_TRUE(identity.has_value());
    EXPECT_FALSE(identity->email_verified);
}

TEST_F(GoogleIdTokenVerifierTest, AcceptsEmailVerifiedSentAsTheStringTrue) {
    auto builder = jwt::create()
                       .set_key_id(key_.kid)
                       .set_issuer("https://accounts.google.com")
                       .set_audience(kTestGoogleClientId)
                       .set_issued_at(std::chrono::system_clock::now())
                       .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours{1})
                       .set_subject("google-sub-42")
                       .set_payload_claim("email", jwt::claim(std::string("gina@example.com")))
                       .set_payload_claim("email_verified", jwt::claim(std::string("true")));
    const std::string token = builder.sign(jwt::algorithm::rs256("", key_.private_pem));

    const auto identity = verifier_.verify(token);

    ASSERT_TRUE(identity.has_value());
    EXPECT_TRUE(identity->email_verified);
}

}  // namespace
