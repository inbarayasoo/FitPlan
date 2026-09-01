// End-to-end tests for the auth flow: a real FitPlanApp is started on a loopback
// port in a background thread, and each case talks to it over a real TCP socket.
// This exercises the whole stack - global middleware, routing, controller, DTOs,
// service, repository, SQLite - the way an HTTP client would.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <thread>

#include "app/App.hpp"
#include "controllers/AuthController.hpp"
#include "db/Database.hpp"
#include "HttpTestClient.hpp"
#include "middleware/JwtAuthMiddleware.hpp"
#include "repositories/UserRepository.hpp"
#include "services/AuthService.hpp"

namespace {

using fitplan::testutil::http_request;
using fitplan::testutil::HttpResponse;
using fitplan::testutil::json_string;

std::string migrations_dir() {
    return FITPLAN_TEST_MIGRATIONS_DIR;
}

class AuthFlowTest : public ::testing::Test {
protected:
    void SetUp() override {
        app_.get_middleware<fitplan::middleware::JwtAuthMiddleware>().secret = kSecret;
        fitplan::controllers::register_auth_routes(app_, auth_);
        app_.bindaddr("127.0.0.1").port(0);  // 0 -> the OS picks a free port
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

    HttpResponse post(const std::string& path, const std::string& body,
                      const std::string& bearer = "") {
        return http_request(port_, "POST", path, body, bearer);
    }
    HttpResponse get(const std::string& path, const std::string& bearer = "") {
        return http_request(port_, "GET", path, "", bearer);
    }

    static constexpr const char* kSecret = "integration-secret";
    static constexpr const char* kCoachBody =
        R"({"email":"coach@itest.com","password":"password123","role":"coach","display_name":"Coach I"})";

    fitplan::db::Database db_{":memory:", migrations_dir()};
    fitplan::repositories::UserRepository users_{db_.connection()};
    fitplan::services::AuthService auth_{users_, kSecret, 3600};
    fitplan::app::FitPlanApp app_;
    std::thread server_;
    std::uint16_t port_ = 0;
};

TEST_F(AuthFlowTest, RegisterThenLoginThenMe) {
    const auto reg = post("/api/auth/register", kCoachBody);
    EXPECT_EQ(reg.status, 201);
    EXPECT_FALSE(json_string(reg.body, "access_token").empty());

    const auto login =
        post("/api/auth/login", R"({"email":"coach@itest.com","password":"password123"})");
    ASSERT_EQ(login.status, 200);
    const std::string token = json_string(login.body, "access_token");
    ASSERT_FALSE(token.empty());

    const auto me = get("/api/auth/me", token);
    EXPECT_EQ(me.status, 200);
    EXPECT_EQ(json_string(me.body, "email"), "coach@itest.com");
    EXPECT_EQ(json_string(me.body, "role"), "coach");
    EXPECT_EQ(me.body.find("password_hash"), std::string::npos);
}

TEST_F(AuthFlowTest, RegisterRejectsADuplicateEmail) {
    ASSERT_EQ(post("/api/auth/register", kCoachBody).status, 201);
    EXPECT_EQ(post("/api/auth/register", kCoachBody).status, 409);
}

TEST_F(AuthFlowTest, RegisterRejectsAnInvalidBody) {
    EXPECT_EQ(
        post("/api/auth/register",
             R"({"email":"x@itest.com","password":"short","role":"coach","display_name":"X"})")
            .status,
        400);
    EXPECT_EQ(post("/api/auth/register", "not json").status, 400);
}

TEST_F(AuthFlowTest, LoginRejectsAWrongPassword) {
    ASSERT_EQ(post("/api/auth/register", kCoachBody).status, 201);
    EXPECT_EQ(post("/api/auth/login", R"({"email":"coach@itest.com","password":"nope"})").status,
              401);
}

TEST_F(AuthFlowTest, MeRequiresAToken) {
    EXPECT_EQ(get("/api/auth/me").status, 401);
}

TEST_F(AuthFlowTest, MeRejectsAGarbageToken) {
    EXPECT_EQ(get("/api/auth/me", "not.a.jwt").status, 401);
}

}  // namespace
