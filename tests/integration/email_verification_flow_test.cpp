// End-to-end tests for the email-verification flow over a real server: the
// scenarios that need control of the clock or many requests in a row (expiry,
// the attempt lockout, resend rotation). The simpler happy-path / status-code
// cases live in auth_flow_test.cpp.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <thread>

#include "app/App.hpp"
#include "controllers/AuthController.hpp"
#include "db/Database.hpp"
#include "HttpTestClient.hpp"
#include "middleware/JwtAuthMiddleware.hpp"
#include "repositories/EmailVerificationTokenRepository.hpp"
#include "repositories/UserRepository.hpp"
#include "services/AuthService.hpp"
#include "services/EmailVerificationService.hpp"
#include "util/Clock.hpp"

namespace {

using fitplan::testutil::CapturingEmailSender;
using fitplan::testutil::http_request;
using fitplan::testutil::HttpResponse;

std::string migrations_dir() {
    return FITPLAN_TEST_MIGRATIONS_DIR;
}

class EmailVerificationFlowTest : public ::testing::Test {
protected:
    void SetUp() override {
        app_.get_middleware<fitplan::middleware::JwtAuthMiddleware>().secret = kSecret;
        fitplan::controllers::register_auth_routes(app_, auth_);
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

    int register_ev() {
        return post("/api/auth/register", R"({"email":"ev@it.com","password":"password123",)"
                                          R"("role":"trainee","display_name":"Ev"})")
            .status;
    }

    int verify(const std::string& code) {
        return post("/api/auth/verify-email", R"({"email":"ev@it.com","code":")" + code + R"("})")
            .status;
    }

    void advance_seconds(std::int64_t s) { now_ = fitplan::util::iso_utc_shift(now_, s); }

    static constexpr const char* kSecret = "it-ev-secret";

    std::string now_ = "2026-09-02 12:00:00";

    fitplan::db::Database db_{":memory:", migrations_dir()};
    fitplan::repositories::UserRepository users_{db_.connection()};
    CapturingEmailSender mail_;
    fitplan::repositories::EmailVerificationTokenRepository ev_tokens_{db_.connection()};
    fitplan::services::EmailVerificationService ev_{users_, ev_tokens_, mail_.sender(),
                                                    [this] { return now_; }, "http://itest"};
    fitplan::services::AuthService auth_{users_, kSecret, 3600, nullptr, &ev_};
    fitplan::app::FitPlanApp app_;
    std::thread server_;
    std::uint16_t port_ = 0;
};

TEST_F(EmailVerificationFlowTest, ACodeStopsWorkingAfterTenMinutes) {
    ASSERT_EQ(register_ev(), 201);
    const std::string code = mail_.last_code();

    advance_seconds(600);

    EXPECT_EQ(verify(code), 410);  // expired
    EXPECT_EQ(verify(code), 404);  // and burned - nothing pending now
}

TEST_F(EmailVerificationFlowTest, FiveWrongCodesLockTheCodeOut) {
    ASSERT_EQ(register_ev(), 201);
    const std::string good = mail_.last_code();
    const std::string bad = good == "000000" ? "111111" : "000000";

    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(verify(bad), 400) << "attempt " << i;
    }
    EXPECT_EQ(verify(good), 429);  // limit reached, even the right code is refused
    EXPECT_EQ(verify(good), 404);  // burned
}

TEST_F(EmailVerificationFlowTest, ResendRotatesTheCodeAndResetsTheAttemptCounter) {
    ASSERT_EQ(register_ev(), 201);
    const std::string first = mail_.last_code();
    const std::string bad = first == "000000" ? "111111" : "000000";
    EXPECT_EQ(verify(bad), 400);
    EXPECT_EQ(verify(bad), 400);

    advance_seconds(60);
    EXPECT_EQ(post("/api/auth/resend-verification", R"({"email":"ev@it.com"})").status, 202);
    ASSERT_EQ(mail_.messages.size(), 2u);
    const std::string second = mail_.last_code();

    if (second != first) {
        EXPECT_EQ(verify(first), 400);  // the old code is dead
    }
    EXPECT_EQ(verify(second), 200);  // fresh code works despite the earlier wrong tries
}

TEST_F(EmailVerificationFlowTest, ResendInsideTheCooldownDoesNotSendAgain) {
    ASSERT_EQ(register_ev(), 201);
    ASSERT_EQ(mail_.messages.size(), 1u);

    EXPECT_EQ(post("/api/auth/resend-verification", R"({"email":"ev@it.com"})").status, 202);
    EXPECT_EQ(mail_.messages.size(), 1u);  // too soon - silent

    advance_seconds(60);
    EXPECT_EQ(post("/api/auth/resend-verification", R"({"email":"ev@it.com"})").status, 202);
    EXPECT_EQ(mail_.messages.size(), 2u);
}

}  // namespace
