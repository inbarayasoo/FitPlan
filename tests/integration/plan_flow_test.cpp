// End-to-end tests for the Step 4 roster + workout-plan API: real server, real
// sockets. Covers attach-by-email, nested-item plan creation, the delete-and-
// reinsert update, single-active-plan assignment, and the full authz matrix.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <thread>

#include "app/App.hpp"
#include "controllers/AuthController.hpp"
#include "controllers/ExerciseController.hpp"
#include "controllers/PlanController.hpp"
#include "controllers/TraineeController.hpp"
#include "db/Database.hpp"
#include "HttpTestClient.hpp"
#include "middleware/JwtAuthMiddleware.hpp"
#include "repositories/CoachTraineeRepository.hpp"
#include "repositories/EmailVerificationTokenRepository.hpp"
#include "repositories/ExerciseRepository.hpp"
#include "repositories/PlanItemRepository.hpp"
#include "repositories/PlanRepository.hpp"
#include "repositories/UserRepository.hpp"
#include "services/AuthService.hpp"
#include "services/EmailVerificationService.hpp"
#include "services/PlanService.hpp"
#include "util/Clock.hpp"

namespace {

using fitplan::testutil::http_request;
using fitplan::testutil::HttpResponse;
using fitplan::testutil::json_number;
using fitplan::testutil::json_string;

std::string migrations_dir() {
    return FITPLAN_TEST_MIGRATIONS_DIR;
}

class PlanFlowTest : public ::testing::Test {
protected:
    void SetUp() override {
        app_.get_middleware<fitplan::middleware::JwtAuthMiddleware>().secret = kSecret;
        fitplan::controllers::register_auth_routes(app_, auth_);
        fitplan::controllers::register_exercise_routes(app_, exercises_);
        fitplan::controllers::register_plan_routes(app_, plans_);
        fitplan::controllers::register_trainee_routes(app_, users_, roster_);
        app_.bindaddr("127.0.0.1").port(0);
        server_ = std::thread([this] { app_.run(); });
        app_.wait_for_server_start();
        port_ = app_.port();

        coach_a_ = reg("a@it.com", "coach");
        coach_b_ = reg("b@it.com", "coach");
        t1_ = reg("t1@it.com", "trainee");
        t2_ = reg("t2@it.com", "trainee");
        t3_ = reg("t3@it.com", "trainee");
        t1_id_ = user_id(t1_);
        t3_id_ = user_id(t3_);

        // A's roster: t1, t2 (not t3). A's library: ex_a1, ex_a2. B owns ex_b.
        ASSERT_EQ(req("POST", "/api/trainees", R"({"email":"t1@it.com"})", coach_a_).status, 201);
        ASSERT_EQ(req("POST", "/api/trainees", R"({"email":"t2@it.com"})", coach_a_).status, 201);
        ex_a1_ = make_exercise(coach_a_, "Back Squat");
        ex_a2_ = make_exercise(coach_a_, "Bench Press");
        ex_b_ = make_exercise(coach_b_, "Bicep Curl");
    }

    void TearDown() override {
        app_.stop();
        if (server_.joinable())
            server_.join();
    }

    std::string reg(const std::string& email, const std::string& role) {
        return fitplan::testutil::register_and_verify(port_, mail_, email, role);
    }

    HttpResponse req(const std::string& method, const std::string& path, const std::string& body,
                     const std::string& tok) {
        return http_request(port_, method, path, body, tok);
    }

    long long user_id(const std::string& token) {
        return json_number(req("GET", "/api/auth/me", "", token).body, "id");
    }

    long long make_exercise(const std::string& tok, const std::string& name) {
        auto r = req("POST", "/api/exercises", R"({"name":")" + name + R"("})", tok);
        EXPECT_EQ(r.status, 201) << r.body;
        return json_number(r.body, "id");
    }

    // A valid two-item plan body for trainee t1_id_.
    std::string plan_body(const std::string& name) {
        return R"({"trainee_id":)" + std::to_string(t1_id_) + R"(,"name":")" + name +
               R"(","items":[)"
               R"({"exercise_id":)" +
               std::to_string(ex_a1_) +
               R"(,"day_label":"A","target_sets":5,"target_reps":5,"target_weight":100,"rest_seconds":180},)"
               R"({"exercise_id":)" +
               std::to_string(ex_a2_) +
               R"(,"target_sets":3,"target_reps":8,"video_url":"https://youtu.be/dQw4w9WgXcQ"}]})";
    }

    static constexpr const char* kSecret = "it-secret";

    fitplan::db::Database db_{":memory:", migrations_dir()};
    fitplan::repositories::UserRepository users_{db_.connection()};
    fitplan::testutil::CapturingEmailSender mail_;
    fitplan::repositories::EmailVerificationTokenRepository ev_tokens_{db_.connection()};
    fitplan::services::EmailVerificationService ev_{users_, ev_tokens_, mail_.sender(),
                                                    fitplan::util::iso_utc_now, "http://itest"};
    fitplan::repositories::ExerciseRepository exercises_{db_.connection()};
    fitplan::repositories::PlanRepository plan_repo_{db_.connection()};
    fitplan::repositories::PlanItemRepository plan_items_{db_.connection()};
    fitplan::repositories::CoachTraineeRepository roster_{db_.connection()};
    fitplan::services::AuthService auth_{users_, kSecret, 3600, nullptr, &ev_};
    fitplan::services::PlanService plans_{db_.connection(), plan_repo_, plan_items_, roster_,
                                          exercises_};
    fitplan::app::FitPlanApp app_;
    std::thread server_;
    std::uint16_t port_ = 0;
    std::string coach_a_, coach_b_, t1_, t2_, t3_;
    long long t1_id_ = 0, t3_id_ = 0;
    long long ex_a1_ = 0, ex_a2_ = 0, ex_b_ = 0;
};

TEST_F(PlanFlowTest, AttachTraineeByEmail) {
    EXPECT_EQ(req("POST", "/api/trainees", R"({"email":"t1@it.com"})", coach_a_).status,
              409);  // already attached in SetUp
    EXPECT_EQ(req("POST", "/api/trainees", R"({"email":"ghost@it.com"})", coach_a_).status, 404);
    EXPECT_EQ(req("POST", "/api/trainees", R"({"email":"b@it.com"})", coach_a_).status,
              400);  // that account is a coach

    auto roster_a = req("GET", "/api/trainees", "", coach_a_);
    EXPECT_EQ(roster_a.status, 200);
    EXPECT_NE(roster_a.body.find("t1@it.com"), std::string::npos);
    EXPECT_NE(roster_a.body.find("t2@it.com"), std::string::npos);
    EXPECT_EQ(roster_a.body.find("password_hash"), std::string::npos);

    auto roster_b = req("GET", "/api/trainees", "", coach_b_);
    EXPECT_EQ(roster_b.status, 200);
    EXPECT_EQ(roster_b.body.find("t1@it.com"), std::string::npos);

    // Remove t1 from A's roster; the second delete 404s, t2 is untouched.
    const std::string t1_path = "/api/trainees/" + std::to_string(t1_id_);
    EXPECT_EQ(req("DELETE", t1_path, "", coach_a_).status, 204);
    EXPECT_EQ(req("DELETE", t1_path, "", coach_a_).status, 404);
    EXPECT_EQ(req("DELETE", t1_path, "", t2_).status, 403);  // trainees can't

    auto roster_after = req("GET", "/api/trainees", "", coach_a_);
    EXPECT_EQ(roster_after.body.find("t1@it.com"), std::string::npos);
    EXPECT_NE(roster_after.body.find("t2@it.com"), std::string::npos);
}

TEST_F(PlanFlowTest, CreateReadListAndCrossCoachIsolation) {
    auto created = req("POST", "/api/plans", plan_body("Week 1"), coach_a_);
    ASSERT_EQ(created.status, 201) << created.body;
    const long long plan_id = json_number(created.body, "id");
    EXPECT_NE(created.body.find(R"("order_index":0)"), std::string::npos);
    EXPECT_NE(created.body.find(R"("order_index":1)"), std::string::npos);
    EXPECT_NE(created.body.find("youtube-nocookie.com/embed/dQw4w9WgXcQ"), std::string::npos);
    EXPECT_NE(created.body.find(R"("is_active":false)"), std::string::npos);

    const std::string path = "/api/plans/" + std::to_string(plan_id);
    EXPECT_EQ(req("GET", path, "", coach_a_).status, 200);

    auto list = req("GET", "/api/plans", "", coach_a_);
    EXPECT_EQ(list.status, 200);
    EXPECT_NE(list.body.find("Week 1"), std::string::npos);
    EXPECT_EQ(list.body.find("order_index"), std::string::npos);  // headers only

    EXPECT_EQ(req("GET", path, "", coach_b_).status, 404);
    EXPECT_EQ(req("PUT", path, plan_body("hijack"), coach_b_).status, 404);

    EXPECT_EQ(req("DELETE", path, "", coach_b_).status, 404);  // not coach B's
    EXPECT_EQ(req("DELETE", path, "", t1_).status, 403);       // trainees can't
    EXPECT_EQ(req("DELETE", path, "", coach_a_).status, 204);  // owner deletes
    EXPECT_EQ(req("GET", path, "", coach_a_).status, 404);     // and it is gone
    EXPECT_EQ(req("DELETE", path, "", coach_a_).status, 404);  // nothing to redo
}

TEST_F(PlanFlowTest, CreateRejectsRosterAndOwnershipAndValidationFailures) {
    // trainee not on A's roster
    const std::string t3_body = R"({"trainee_id":)" + std::to_string(t3_id_) +
                                R"(,"name":"x","items":[{"exercise_id":)" + std::to_string(ex_a1_) +
                                R"(}]})";
    EXPECT_EQ(req("POST", "/api/plans", t3_body, coach_a_).status, 403);

    // item references coach B's exercise
    const std::string foreign_item = R"({"trainee_id":)" + std::to_string(t1_id_) +
                                     R"(,"name":"x","items":[{"exercise_id":)" +
                                     std::to_string(ex_b_) + R"(}]})";
    EXPECT_EQ(req("POST", "/api/plans", foreign_item, coach_a_).status, 403);

    const std::string base = R"({"trainee_id":)" + std::to_string(t1_id_) + ",";
    EXPECT_EQ(req("POST", "/api/plans", base + R"("name":"x","items":[]})", coach_a_).status, 400);
    EXPECT_EQ(req("POST", "/api/plans",
                  base + R"("name":"  ","items":[{"exercise_id":)" + std::to_string(ex_a1_) + "}]}",
                  coach_a_)
                  .status,
              400);
    EXPECT_EQ(req("POST", "/api/plans",
                  base + R"("name":"x","items":[{"exercise_id":)" + std::to_string(ex_a1_) +
                      R"(,"target_sets":0}]})",
                  coach_a_)
                  .status,
              400);
    EXPECT_EQ(req("POST", "/api/plans",
                  base + R"("name":"x","items":[{"exercise_id":)" + std::to_string(ex_a1_) +
                      R"(,"video_url":"https://vimeo.com/1"}]})",
                  coach_a_)
                  .status,
              400);
}

TEST_F(PlanFlowTest, UpdateReplacesTheItemList) {
    auto created = req("POST", "/api/plans", plan_body("Week 1"), coach_a_);
    const long long plan_id = json_number(created.body, "id");
    const std::string path = "/api/plans/" + std::to_string(plan_id);

    const std::string one_item = R"({"trainee_id":)" + std::to_string(t1_id_) +
                                 R"(,"name":"Week 1 - Lower","items":[{"exercise_id":)" +
                                 std::to_string(ex_a1_) + R"(,"target_sets":4,"target_reps":6}]})";
    auto updated = req("PUT", path, one_item, coach_a_);
    ASSERT_EQ(updated.status, 200) << updated.body;
    EXPECT_EQ(json_string(updated.body, "name"), "Week 1 - Lower");
    EXPECT_NE(updated.body.find(R"("order_index":0)"), std::string::npos);
    EXPECT_EQ(updated.body.find(R"("order_index":1)"), std::string::npos);  // dropped
    EXPECT_EQ(updated.body.find("Bench Press"), std::string::npos);

    auto fetched = req("GET", path, "", coach_a_);
    EXPECT_EQ(fetched.body.find(R"("exercise_id":)" + std::to_string(ex_a2_)), std::string::npos);
}

TEST_F(PlanFlowTest, AssignKeepsExactlyOneActivePlanPerTrainee) {
    const long long p1 =
        json_number(req("POST", "/api/plans", plan_body("Week 1"), coach_a_).body, "id");
    const long long p2 =
        json_number(req("POST", "/api/plans", plan_body("Week 2"), coach_a_).body, "id");

    auto a1 = req("POST", "/api/plans/" + std::to_string(p1) + "/assign", "", coach_a_);
    EXPECT_EQ(a1.status, 200);
    EXPECT_NE(a1.body.find(R"("is_active":true)"), std::string::npos);

    EXPECT_EQ(req("POST", "/api/plans/" + std::to_string(p2) + "/assign", "", coach_a_).status,
              200);

    auto g1 = req("GET", "/api/plans/" + std::to_string(p1), "", coach_a_);
    auto g2 = req("GET", "/api/plans/" + std::to_string(p2), "", coach_a_);
    EXPECT_NE(g1.body.find(R"("is_active":false)"), std::string::npos);
    EXPECT_NE(g2.body.find(R"("is_active":true)"), std::string::npos);

    // assigning a plan that is not this coach's
    EXPECT_EQ(req("POST", "/api/plans/" + std::to_string(p1) + "/assign", "", coach_b_).status,
              404);
}

TEST_F(PlanFlowTest, RoleAndTokenAreEnforcedOnPlanRoutes) {
    EXPECT_EQ(req("GET", "/api/plans", "", t1_).status, 403);
    EXPECT_EQ(req("POST", "/api/plans", plan_body("x"), t1_).status, 403);
    EXPECT_EQ(req("GET", "/api/trainees", "", t1_).status, 403);
    EXPECT_EQ(http_request(port_, "GET", "/api/plans", "").status, 401);
}

}  // namespace
