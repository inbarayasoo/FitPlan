// End-to-end tests for Google Sign-In: a real FitPlanApp is started on a loopback
// port, but its GoogleIdTokenVerifier is wired to a JWKS cache backed by a local
// RSA test key, so the tests mint their own "Google" tokens and nothing touches
// the network.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

#include "app/App.hpp"
#include "controllers/AuthController.hpp"
#include "db/Database.hpp"
#include "GoogleTokenMint.hpp"
#include "HttpTestClient.hpp"
#include "middleware/JwtAuthMiddleware.hpp"
#include "repositories/UserRepository.hpp"
#include "RsaTestKey.hpp"
#include "services/AuthService.hpp"
#include "services/GoogleIdTokenVerifier.hpp"
#include "services/GoogleJwks.hpp"

namespace {

using fitplan::testutil::GoogleTokenOptions;
using fitplan::testutil::http_request;
using fitplan::testutil::HttpResponse;
using fitplan::testutil::json_number;
using fitplan::testutil::json_string;
using fitplan::testutil::kTestGoogleClientId;
using fitplan::testutil::make_rsa_test_key;
using fitplan::testutil::mint_google_id_token;
using fitplan::testutil::RsaTestKey;

std::string migrations_dir() {
    return FITPLAN_TEST_MIGRATIONS_DIR;
}

class GoogleFlowTest : public ::testing::Test {
protected:
    void SetUp() override {
        app_.get_middleware<fitplan::middleware::JwtAuthMiddleware>().secret = kSecret;
        fitplan::controllers::register_auth_routes(app_, auth_, kTestGoogleClientId);
        app_.bindaddr("127.0.0.1").port(0);
        server_ = std::thread([this] { app_.run(); });
        app_.wait_for_server_start();
        port_ = app_.port();
    }

    void TearDown() override {
        app_.stop();
        if (server_.joinable()) {
            server_.join();
        }
    }

    HttpResponse post(const std::string& path, const std::string& body) {
        return http_request(port_, "POST", path, body);
    }
    HttpResponse get(const std::string& path, const std::string& bearer = "") {
        return http_request(port_, "GET", path, "", bearer);
    }

    static std::string google_body(const std::string& id_token, const std::string& role = "") {
        if (role.empty()) {
            return R"({"id_token":")" + id_token + R"("})";
        }
        return R"({"id_token":")" + id_token + R"(","role":")" + role + R"("})";
    }

    static constexpr const char* kSecret = "integration-secret";
    static constexpr const char* kRegisterCoach =
        R"({"email":"coach@itest.com","password":"password123","role":"coach","display_name":"Coach"})";

    RsaTestKey key_ = make_rsa_test_key("itest-google-kid");
    fitplan::db::Database db_{":memory:", migrations_dir()};
    fitplan::repositories::UserRepository users_{db_.connection()};
    fitplan::services::GoogleJwksCache jwks_{[this] {
        fitplan::services::JwksDocument doc;
        doc.body = RsaTestKey::jwks_document({key_});
        return doc;
    }};
    fitplan::services::GoogleIdTokenVerifier verifier_{kTestGoogleClientId, jwks_};
    fitplan::services::AuthService auth_{users_, kSecret, 3600, &verifier_};
    fitplan::app::FitPlanApp app_;
    std::thread server_;
    std::uint16_t port_ = 0;
};

TEST_F(GoogleFlowTest, ConfigReportsTheClientId) {
    const auto res = get("/api/auth/config");
    EXPECT_EQ(res.status, 200);
    EXPECT_EQ(json_string(res.body, "google_client_id"), kTestGoogleClientId);
}

TEST_F(GoogleFlowTest, FirstGoogleLoginWithoutARoleAsksForOneThenCreates) {
    const std::string id_token = mint_google_id_token(key_);

    const auto ask = post("/api/auth/google", google_body(id_token));
    ASSERT_EQ(ask.status, 200);
    EXPECT_NE(ask.body.find(R"("needs_role":true)"), std::string::npos);
    EXPECT_EQ(ask.body.find("access_token"), std::string::npos);

    const auto done = post("/api/auth/google", google_body(id_token, "trainee"));
    ASSERT_EQ(done.status, 200);
    const std::string token = json_string(done.body, "access_token");
    ASSERT_FALSE(token.empty());
    EXPECT_NE(done.body.find(R"("email":"gina@example.com")"), std::string::npos);
    EXPECT_NE(done.body.find(R"("auth_provider":"google")"), std::string::npos);
    EXPECT_NE(done.body.find(R"("role":"trainee")"), std::string::npos);
    EXPECT_EQ(done.body.find("password_hash"), std::string::npos);

    // The FitPlan token it returns works on a normal protected route.
    const auto me = get("/api/auth/me", token);
    EXPECT_EQ(me.status, 200);
    EXPECT_EQ(json_string(me.body, "email"), "gina@example.com");
    EXPECT_EQ(json_string(me.body, "auth_provider"), "google");
}

TEST_F(GoogleFlowTest, ACoachRoleOnTheFirstCallCreatesACoach) {
    const auto res = post("/api/auth/google", google_body(mint_google_id_token(key_), "coach"));

    ASSERT_EQ(res.status, 200);
    EXPECT_NE(res.body.find(R"("role":"coach")"), std::string::npos);
}

TEST_F(GoogleFlowTest, SecondGoogleLoginIgnoresTheRoleAndReturnsTheSameAccount) {
    const auto first = post("/api/auth/google", google_body(mint_google_id_token(key_), "trainee"));
    const auto second = post("/api/auth/google", google_body(mint_google_id_token(key_), "coach"));
    ASSERT_EQ(first.status, 200);
    ASSERT_EQ(second.status, 200);

    EXPECT_EQ(json_number(first.body, "id"), json_number(second.body, "id"));
    EXPECT_NE(second.body.find(R"("role":"trainee")"), std::string::npos);
}

TEST_F(GoogleFlowTest, GoogleLoginLinksToAPasswordAccountWithTheSameVerifiedEmail) {
    const auto reg = post("/api/auth/register", kRegisterCoach);
    ASSERT_EQ(reg.status, 201);
    const long long local_id = json_number(reg.body, "id");

    GoogleTokenOptions opt;
    opt.email = "coach@itest.com";
    opt.subject = "sub-for-coach";
    // The link path never asks for a role, even when none is sent.
    const auto res = post("/api/auth/google", google_body(mint_google_id_token(key_, opt)));

    ASSERT_EQ(res.status, 200);
    EXPECT_EQ(res.body.find("needs_role"), std::string::npos);
    EXPECT_EQ(json_number(res.body, "id"), local_id);
    EXPECT_NE(res.body.find(R"("role":"coach")"), std::string::npos);  // role is kept
}

TEST_F(GoogleFlowTest, RefusesToLinkWhenTheGoogleEmailIsNotVerified) {
    ASSERT_EQ(post("/api/auth/register", kRegisterCoach).status, 201);

    GoogleTokenOptions opt;
    opt.email = "coach@itest.com";
    opt.email_verified = false;
    const auto res = post("/api/auth/google", google_body(mint_google_id_token(key_, opt)));

    EXPECT_EQ(res.status, 409);
}

TEST_F(GoogleFlowTest, RejectsAnInvalidToken) {
    EXPECT_EQ(post("/api/auth/google", google_body("not.a.real.token")).status, 401);
}

TEST_F(GoogleFlowTest, RejectsAnExpiredToken) {
    GoogleTokenOptions opt;
    opt.lifetime = -std::chrono::minutes{5};
    EXPECT_EQ(post("/api/auth/google", google_body(mint_google_id_token(key_, opt))).status, 401);
}

TEST_F(GoogleFlowTest, RejectsAMalformedBody) {
    EXPECT_EQ(post("/api/auth/google", "not json").status, 400);
    EXPECT_EQ(post("/api/auth/google", R"({"wrong":"field"})").status, 400);
}

}  // namespace
