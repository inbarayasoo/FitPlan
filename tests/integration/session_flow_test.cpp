// End-to-end tests for the Step 5 trainee API + progress engine: real server,
// real sockets. Covers GET /my/plan (effective video url), logging a session
// with sets, PATCH, the progress numbers, the coach's roster-scoped view, and
// the authz matrix.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <thread>

#include "HttpTestClient.hpp"
#include "app/App.hpp"
#include "controllers/AuthController.hpp"
#include "controllers/ExerciseController.hpp"
#include "controllers/PlanController.hpp"
#include "controllers/ProgressController.hpp"
#include "controllers/SessionController.hpp"
#include "controllers/TraineeController.hpp"
#include "db/Database.hpp"
#include "middleware/JwtAuthMiddleware.hpp"
#include "repositories/CoachTraineeRepository.hpp"
#include "repositories/ExerciseRepository.hpp"
#include "repositories/PlanItemRepository.hpp"
#include "repositories/PlanRepository.hpp"
#include "repositories/SessionRepository.hpp"
#include "repositories/SessionSetRepository.hpp"
#include "repositories/UserRepository.hpp"
#include "services/AuthService.hpp"
#include "services/PlanService.hpp"
#include "services/SessionService.hpp"

namespace {

using fitplan::testutil::HttpResponse;
using fitplan::testutil::http_request;
using fitplan::testutil::json_number;
using fitplan::testutil::json_string;

std::string migrations_dir() { return FITPLAN_TEST_MIGRATIONS_DIR; }

class SessionFlowTest : public ::testing::Test {
protected:
    void SetUp() override {
        app_.get_middleware<fitplan::middleware::JwtAuthMiddleware>().secret =
            kSecret;
        fitplan::controllers::register_auth_routes(app_, auth_);
        fitplan::controllers::register_exercise_routes(app_, exercises_);
        fitplan::controllers::register_plan_routes(app_, plans_);
        fitplan::controllers::register_trainee_routes(app_, users_, roster_);
        fitplan::controllers::register_session_routes(app_, session_svc_);
        fitplan::controllers::register_progress_routes(app_, session_svc_,
                                                       roster_);
        app_.bindaddr("127.0.0.1").port(0);
        server_ = std::thread([this] { app_.run(); });
        app_.wait_for_server_start();
        port_ = app_.port();

        coach_ = reg("coach@it.com", "coach");
        trainee_ = reg("t@it.com", "trainee");
        other_ = reg("o@it.com", "trainee");
        trainee_id_ = user_id(trainee_);

        ASSERT_EQ(req("POST", "/api/trainees", R"({"email":"t@it.com"})", coach_)
                      .status,
                  201);
        ex1_ = make_exercise(
            R"({"name":"Squat","video_url":"https://www.youtube.com/watch?v=ultWZbUMPL8"})");
        ex2_ = make_exercise(R"({"name":"Bench"})");

        const std::string plan_body =
            R"({"trainee_id":)" + std::to_string(trainee_id_) +
            R"(,"name":"W1","items":[)"
            R"({"exercise_id":)" + std::to_string(ex1_) +
            R"(,"target_sets":3,"target_reps":5},)"
            R"({"exercise_id":)" + std::to_string(ex2_) +
            R"(,"target_sets":2,"target_reps":8}]})";
        auto created = req("POST", "/api/plans", plan_body, coach_);
        ASSERT_EQ(created.status, 201) << created.body;
        plan_id_ = json_number(created.body, "id");
        // header id is the first "id"; the two item ids follow.
        item1_ = nth_number(created.body, "id", 2);
        item2_ = nth_number(created.body, "id", 3);
        ASSERT_EQ(req("POST",
                      "/api/plans/" + std::to_string(plan_id_) + "/assign", "",
                      coach_)
                      .status,
                  200);
    }

    void TearDown() override {
        app_.stop();
        if (server_.joinable()) server_.join();
    }

    std::string reg(const std::string& email, const std::string& role) {
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

    long long user_id(const std::string& token) {
        return json_number(req("GET", "/api/auth/me", "", token).body, "id");
    }

    long long make_exercise(const std::string& body) {
        auto r = req("POST", "/api/exercises", body, coach_);
        EXPECT_EQ(r.status, 201) << r.body;
        return json_number(r.body, "id");
    }

    // The Nth (1-based) integer value for "key": in a JSON body.
    static long long nth_number(const std::string& body, const std::string& key,
                                int n) {
        const std::string needle = "\"" + key + "\":";
        std::size_t pos = 0;
        for (int i = 0; i < n; ++i) {
            pos = body.find(needle, i == 0 ? 0 : pos + 1);
            if (pos == std::string::npos) return -1;
        }
        return std::strtoll(body.c_str() + pos + needle.size(), nullptr, 10);
    }

    std::string log_body_last_week() const { return log_body("2026-08-23 09:00:00"); }

    // A 5-set session (3x squat @100, 2x bench @60). `performed_at` empty => the
    // server defaults it to now, which keeps it inside the weekly-streak window.
    std::string log_body(const std::string& performed_at) const {
        const std::string when =
            performed_at.empty()
                ? std::string()
                : R"("performed_at":")" + performed_at + R"(",)";
        return R"({"plan_id":)" + std::to_string(plan_id_) + R"(,)" + when +
               R"("sets":[)"
               R"({"exercise_id":)" + std::to_string(ex1_) +
               R"(,"plan_item_id":)" + std::to_string(item1_) +
               R"(,"reps":5,"weight":100},)"
               R"({"exercise_id":)" + std::to_string(ex1_) +
               R"(,"plan_item_id":)" + std::to_string(item1_) +
               R"(,"reps":5,"weight":100},)"
               R"({"exercise_id":)" + std::to_string(ex1_) +
               R"(,"plan_item_id":)" + std::to_string(item1_) +
               R"(,"reps":5,"weight":100},)"
               R"({"exercise_id":)" + std::to_string(ex2_) +
               R"(,"plan_item_id":)" + std::to_string(item2_) +
               R"(,"reps":8,"weight":60},)"
               R"({"exercise_id":)" + std::to_string(ex2_) +
               R"(,"plan_item_id":)" + std::to_string(item2_) +
               R"(,"reps":8,"weight":60}]})";
    }

    static constexpr const char* kSecret = "it-secret";

    fitplan::db::Database db_{":memory:", migrations_dir()};
    fitplan::repositories::UserRepository users_{db_.connection()};
    fitplan::repositories::ExerciseRepository exercises_{db_.connection()};
    fitplan::repositories::PlanRepository plan_repo_{db_.connection()};
    fitplan::repositories::PlanItemRepository plan_items_{db_.connection()};
    fitplan::repositories::CoachTraineeRepository roster_{db_.connection()};
    fitplan::repositories::SessionRepository sessions_{db_.connection()};
    fitplan::repositories::SessionSetRepository session_sets_{db_.connection()};
    fitplan::services::AuthService auth_{users_, kSecret, 3600};
    fitplan::services::PlanService plans_{db_.connection(), plan_repo_,
                                          plan_items_, roster_, exercises_};
    fitplan::services::SessionService session_svc_{
        db_.connection(), sessions_,   session_sets_,
        plan_repo_,       plan_items_, exercises_};
    fitplan::app::FitPlanApp app_;
    std::thread server_;
    std::uint16_t port_ = 0;
    std::string coach_, trainee_, other_;
    long long trainee_id_ = 0, plan_id_ = 0, item1_ = 0, item2_ = 0;
    long long ex1_ = 0, ex2_ = 0;
};

TEST_F(SessionFlowTest, MyPlanReturnsActivePlanWithEffectiveVideoUrl) {
    auto r = req("GET", "/api/my/plan", "", trainee_);
    ASSERT_EQ(r.status, 200) << r.body;
    // item 1 has no override, so it inherits the exercise's library link
    EXPECT_NE(r.body.find("youtube-nocookie.com/embed/ultWZbUMPL8"),
              std::string::npos);
    EXPECT_NE(r.body.find(R"("is_active":true)"), std::string::npos);
}

TEST_F(SessionFlowTest, MyPlanIs404WhenNothingActive) {
    EXPECT_EQ(req("GET", "/api/my/plan", "", other_).status, 404);
}

TEST_F(SessionFlowTest, LogListAndPatchASession) {
    auto logged = req("POST", "/api/my/sessions", log_body_last_week(), trainee_);
    ASSERT_EQ(logged.status, 201) << logged.body;
    EXPECT_NE(logged.body.find(R"("set_number":5)"), std::string::npos);
    const long long sid = json_number(logged.body, "id");

    auto list = req("GET", "/api/my/sessions", "", trainee_);
    EXPECT_EQ(list.status, 200);
    EXPECT_NE(list.body.find(R"("status":"completed")"), std::string::npos);

    auto patched = req("PATCH", "/api/my/sessions/" + std::to_string(sid),
                       R"({"status":"in_progress","notes":"hard"})", trainee_);
    ASSERT_EQ(patched.status, 200) << patched.body;
    EXPECT_NE(patched.body.find(R"("status":"in_progress")"), std::string::npos);
    EXPECT_NE(patched.body.find(R"("notes":"hard")"), std::string::npos);

    // another trainee cannot patch it
    EXPECT_EQ(req("PATCH", "/api/my/sessions/" + std::to_string(sid),
                  R"({"status":"completed"})", other_)
                  .status,
              404);
}

TEST_F(SessionFlowTest, ProgressNumbersAddUp) {
    // logged "today" so it lands in the current ISO week
    ASSERT_EQ(req("POST", "/api/my/sessions", log_body(""), trainee_).status,
              201);

    auto p = req("GET", "/api/my/progress", "", trainee_);
    ASSERT_EQ(p.status, 200) << p.body;
    // volume = 3*(5*100) + 2*(8*60) = 2460
    EXPECT_NE(p.body.find(R"("total_volume":2460.0)"), std::string::npos);
    // every prescribed set was done -> adherence 1
    EXPECT_NE(p.body.find(R"("adherence":1.0)"), std::string::npos);
    // trained this week -> streak at least 1
    EXPECT_EQ(p.body.find(R"("weekly_streak":0)"), std::string::npos);
    // e1RM for the squat that day: 100 * (1 + 5/30) = 116.666...
    EXPECT_NE(p.body.find("116.66666"), std::string::npos);
}

TEST_F(SessionFlowTest, CoachSeesRosterTraineeProgressButNotOthers) {
    ASSERT_EQ(
        req("POST", "/api/my/sessions", log_body_last_week(), trainee_).status,
        201);

    auto ok = req("GET", "/api/trainees/" + std::to_string(trainee_id_) +
                             "/progress",
                  "", coach_);
    EXPECT_EQ(ok.status, 200) << ok.body;
    EXPECT_NE(ok.body.find(R"("total_volume":2460.0)"), std::string::npos);

    const long long other_id = user_id(other_);
    EXPECT_EQ(req("GET",
                  "/api/trainees/" + std::to_string(other_id) + "/progress", "",
                  coach_)
                  .status,
              404);  // not on the coach's roster
}

TEST_F(SessionFlowTest, ValidationAndRoleMatrix) {
    // unknown exercise in a set
    EXPECT_EQ(req("POST", "/api/my/sessions",
                  R"({"sets":[{"exercise_id":99999}]})", trainee_)
                  .status,
              400);
    // a set links a plan item that is not on the active plan
    EXPECT_EQ(req("POST", "/api/my/sessions",
                  R"({"sets":[{"exercise_id":)" + std::to_string(ex1_) +
                      R"(,"plan_item_id":424242}]})",
                  trainee_)
                  .status,
              403);
    // bad status value
    EXPECT_EQ(req("POST", "/api/my/sessions", R"({"status":"done","sets":[]})",
                  trainee_)
                  .status,
              400);

    // role + token enforcement
    EXPECT_EQ(req("GET", "/api/my/plan", "", coach_).status, 403);
    EXPECT_EQ(req("GET", "/api/my/progress", "", coach_).status, 403);
    EXPECT_EQ(req("GET",
                  "/api/trainees/" + std::to_string(trainee_id_) + "/progress",
                  "", trainee_)
                  .status,
              403);
    EXPECT_EQ(http_request(port_, "GET", "/api/my/sessions", "").status, 401);
}

}  // namespace
