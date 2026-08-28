// End-to-end tests for the auth flow: a real FitPlanApp is started on a loopback
// port in a background thread, and each case talks to it over a real TCP socket.
// This exercises the whole stack - global middleware, routing, controller, DTOs,
// service, repository, SQLite - the way an HTTP client would.

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <string>
#include <thread>

#include "app/App.hpp"
#include "controllers/AuthController.hpp"
#include "db/Database.hpp"
#include "middleware/JwtAuthMiddleware.hpp"
#include "repositories/UserRepository.hpp"
#include "services/AuthService.hpp"

namespace {

std::string migrations_dir() { return FITPLAN_TEST_MIGRATIONS_DIR; }

struct HttpResponse {
    int status = 0;
    std::string body;
};

// Minimal blocking HTTP/1.1 client: connect to 127.0.0.1:port, send one request,
// read the whole response, split off the status code and the body.
HttpResponse http_request(std::uint16_t port, const std::string& method,
                          const std::string& path, const std::string& body,
                          const std::string& bearer = "") {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        ADD_FAILURE() << "socket() failed";
        return {};
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ADD_FAILURE() << "connect() failed";
        ::close(fd);
        return {};
    }

    std::string req = method + " " + path + " HTTP/1.1\r\n";
    req += "Host: 127.0.0.1\r\n";
    req += "Content-Type: application/json\r\n";
    if (!bearer.empty()) {
        req += "Authorization: Bearer " + bearer + "\r\n";
    }
    req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    req += "Connection: close\r\n\r\n";
    req += body;
    ::send(fd, req.data(), req.size(), 0);

    std::string raw;
    char buf[4096];
    ssize_t n = 0;
    while ((n = ::recv(fd, buf, sizeof(buf), 0)) > 0) {
        raw.append(buf, static_cast<std::size_t>(n));
    }
    ::close(fd);

    HttpResponse res;
    const auto first_space = raw.find(' ');
    if (first_space != std::string::npos) {
        res.status = std::stoi(raw.substr(first_space + 1, 3));
    }
    const auto header_end = raw.find("\r\n\r\n");
    if (header_end != std::string::npos) {
        res.body = raw.substr(header_end + 4);
    }
    return res;
}

// Pulls a top-level string value out of a flat JSON object body.
std::string json_string(const std::string& body, const std::string& key) {
    const std::string needle = "\"" + key + "\":\"";
    const auto start = body.find(needle);
    if (start == std::string::npos) {
        return {};
    }
    const auto from = start + needle.size();
    const auto end = body.find('"', from);
    return body.substr(from, end - from);
}

class AuthFlowTest : public ::testing::Test {
protected:
    void SetUp() override {
        app_.get_middleware<fitplan::middleware::JwtAuthMiddleware>().secret =
            kSecret;
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

    const auto login = post("/api/auth/login",
                            R"({"email":"coach@itest.com","password":"password123"})");
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
    EXPECT_EQ(post("/api/auth/register",
                   R"({"email":"x@itest.com","password":"short","role":"coach","display_name":"X"})")
                  .status,
              400);
    EXPECT_EQ(post("/api/auth/register", "not json").status, 400);
}

TEST_F(AuthFlowTest, LoginRejectsAWrongPassword) {
    ASSERT_EQ(post("/api/auth/register", kCoachBody).status, 201);
    EXPECT_EQ(post("/api/auth/login",
                   R"({"email":"coach@itest.com","password":"nope"})")
                  .status,
              401);
}

TEST_F(AuthFlowTest, MeRequiresAToken) {
    EXPECT_EQ(get("/api/auth/me").status, 401);
}

TEST_F(AuthFlowTest, MeRejectsAGarbageToken) {
    EXPECT_EQ(get("/api/auth/me", "not.a.jwt").status, 401);
}

}  // namespace
