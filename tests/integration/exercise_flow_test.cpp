// End-to-end tests for the Step 4 exercise-library API: real server, real
// sockets, full CRUD plus the ownership/authz/validation edges.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <thread>

#include "HttpTestClient.hpp"
#include "app/App.hpp"
#include "controllers/AuthController.hpp"
#include "controllers/ExerciseController.hpp"
#include "db/Database.hpp"
#include "middleware/JwtAuthMiddleware.hpp"
#include "repositories/ExerciseRepository.hpp"
#include "repositories/UserRepository.hpp"
#include "services/AuthService.hpp"

namespace {

using fitplan::testutil::HttpResponse;
using fitplan::testutil::http_request;
using fitplan::testutil::json_number;
using fitplan::testutil::json_string;

std::string migrations_dir() { return FITPLAN_TEST_MIGRATIONS_DIR; }

class ExerciseFlowTest : public ::testing::Test {
protected:
    void SetUp() override {
        app_.get_middleware<fitplan::middleware::JwtAuthMiddleware>().secret =
            kSecret;
        fitplan::controllers::register_auth_routes(app_, auth_);
        fitplan::controllers::register_exercise_routes(app_, exercises_);
        app_.bindaddr("127.0.0.1").port(0);
        server_ = std::thread([this] { app_.run(); });
        app_.wait_for_server_start();
        port_ = app_.port();

        coach_a_ = register_user("a@it.com", "coach");
        coach_b_ = register_user("b@it.com", "coach");
        trainee_ = register_user("t@it.com", "trainee");
    }

    void TearDown() override {
        app_.stop();
        if (server_.joinable()) server_.join();
    }

    std::string register_user(const std::string& email, const std::string& role) {
        const std::string body = R"({"email":")" + email +
                                 R"(","password":"password123","role":")" + role +
                                 R"(","display_name":"U"})";
        auto r = http_request(port_, "POST", "/api/auth/register", body);
        EXPECT_EQ(r.status, 201) << r.body;
        return json_string(r.body, "access_token");
    }

    HttpResponse req(const std::string& method, const std::string& path,
                     const std::string& body, const std::string& tok) {
        return http_request(port_, method, path, body, tok);
    }

    static constexpr const char* kSecret = "it-secret";
    static constexpr const char* kSquat =
        R"({"name":"Back Squat","category":"legs","video_url":"https://www.youtube.com/watch?v=dQw4w9WgXcQ"})";

    fitplan::db::Database db_{":memory:", migrations_dir()};
    fitplan::repositories::UserRepository users_{db_.connection()};
    fitplan::repositories::ExerciseRepository exercises_{db_.connection()};
    fitplan::services::AuthService auth_{users_, kSecret, 3600};
    fitplan::app::FitPlanApp app_;
    std::thread server_;
    std::uint16_t port_ = 0;
    std::string coach_a_, coach_b_, trainee_;
};

TEST_F(ExerciseFlowTest, CreateReadUpdateDelete) {
    auto created = req("POST", "/api/exercises", kSquat, coach_a_);
    ASSERT_EQ(created.status, 201) << created.body;
    const long long id = json_number(created.body, "id");
    EXPECT_GT(id, 0);
    EXPECT_NE(created.body.find("youtube-nocookie.com/embed/dQw4w9WgXcQ"),
              std::string::npos);

    const std::string path = "/api/exercises/" + std::to_string(id);
    EXPECT_EQ(req("GET", path, "", coach_a_).status, 200);

    auto updated = req("PUT", path,
                       R"({"name":"Paused Squat","video_url":"https://youtu.be/dQw4w9WgXcQ"})",
                       coach_a_);
    EXPECT_EQ(updated.status, 200);
    EXPECT_EQ(json_string(updated.body, "name"), "Paused Squat");

    EXPECT_EQ(req("DELETE", path, "", coach_a_).status, 204);
    EXPECT_EQ(req("GET", path, "", coach_a_).status, 404);
    EXPECT_EQ(req("DELETE", path, "", coach_a_).status, 404);
}

TEST_F(ExerciseFlowTest, ListIsScopedToTheOwningCoach) {
    ASSERT_EQ(req("POST", "/api/exercises", kSquat, coach_a_).status, 201);
    ASSERT_EQ(req("POST", "/api/exercises", R"({"name":"Deadlift"})", coach_a_).status,
              201);

    auto list_a = req("GET", "/api/exercises", "", coach_a_);
    EXPECT_EQ(list_a.status, 200);
    EXPECT_NE(list_a.body.find("Back Squat"), std::string::npos);
    EXPECT_NE(list_a.body.find("Deadlift"), std::string::npos);

    auto list_b = req("GET", "/api/exercises", "", coach_b_);
    EXPECT_EQ(list_b.status, 200);
    EXPECT_EQ(list_b.body.find("Back Squat"), std::string::npos);
}

TEST_F(ExerciseFlowTest, OneCoachCannotTouchAnothersRow) {
    auto created = req("POST", "/api/exercises", kSquat, coach_a_);
    const std::string path =
        "/api/exercises/" + std::to_string(json_number(created.body, "id"));

    EXPECT_EQ(req("GET", path, "", coach_b_).status, 404);
    EXPECT_EQ(req("PUT", path, R"({"name":"hijack"})", coach_b_).status, 404);
    EXPECT_EQ(req("DELETE", path, "", coach_b_).status, 404);
    EXPECT_EQ(req("GET", "/api/exercises/999999", "", coach_a_).status, 404);
}

TEST_F(ExerciseFlowTest, RejectsBadInput) {
    EXPECT_EQ(req("POST", "/api/exercises",
                  R"({"name":"x","video_url":"http://youtube.com/watch?v=a"})", coach_a_)
                  .status,
              400);
    EXPECT_EQ(req("POST", "/api/exercises",
                  R"({"name":"x","video_url":"https://vimeo.com/1"})", coach_a_)
                  .status,
              400);
    EXPECT_EQ(req("POST", "/api/exercises", R"({"name":"   "})", coach_a_).status,
              400);
    EXPECT_EQ(req("POST", "/api/exercises", "not json", coach_a_).status, 400);
}

TEST_F(ExerciseFlowTest, RoleAndTokenAreEnforced) {
    EXPECT_EQ(req("GET", "/api/exercises", "", trainee_).status, 403);
    EXPECT_EQ(req("POST", "/api/exercises", kSquat, trainee_).status, 403);
    EXPECT_EQ(http_request(port_, "GET", "/api/exercises", "").status, 401);
    EXPECT_EQ(http_request(port_, "GET", "/api/exercises", "", "not.a.jwt").status,
              401);
}

}  // namespace
