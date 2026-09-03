// End-to-end tests for the Step 6 per-exercise notes API: a trainee keeps their
// own coaching cues against the exercises on their active plan.

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <thread>

#include "app/App.hpp"
#include "controllers/AuthController.hpp"
#include "controllers/ExerciseController.hpp"
#include "controllers/ExerciseNoteController.hpp"
#include "controllers/PlanController.hpp"
#include "controllers/TraineeController.hpp"
#include "db/Database.hpp"
#include "HttpTestClient.hpp"
#include "middleware/JwtAuthMiddleware.hpp"
#include "repositories/CoachTraineeRepository.hpp"
#include "repositories/EmailVerificationTokenRepository.hpp"
#include "repositories/ExerciseNoteRepository.hpp"
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

class ExerciseNoteFlowTest : public ::testing::Test {
protected:
    void SetUp() override {
        app_.get_middleware<fitplan::middleware::JwtAuthMiddleware>().secret = kSecret;
        fitplan::controllers::register_auth_routes(app_, auth_);
        fitplan::controllers::register_exercise_routes(app_, exercises_);
        fitplan::controllers::register_trainee_routes(app_, users_, roster_);
        fitplan::controllers::register_plan_routes(app_, plans_);
        fitplan::controllers::register_exercise_note_routes(app_, notes_, plan_repo_, plan_items_);
        app_.bindaddr("127.0.0.1").port(0);
        server_ = std::thread([this] { app_.run(); });
        app_.wait_for_server_start();
        port_ = app_.port();

        coach_ = reg("c@it.com", "coach");
        trainee_ = reg("t@it.com", "trainee");
        other_ = reg("o@it.com", "trainee");
        trainee_id_ = json_number(req("GET", "/api/auth/me", "", trainee_).body, "id");

        ASSERT_EQ(req("POST", "/api/trainees", R"({"email":"t@it.com"})", coach_).status, 201);
        ex_on_ = make_exercise("Back Squat");
        ex_off_ = make_exercise("Deadlift");  // exists, but not on the plan

        const std::string plan_body = R"({"trainee_id":)" + std::to_string(trainee_id_) +
                                      R"(,"name":"Week 1","items":[{"exercise_id":)" +
                                      std::to_string(ex_on_) + R"(}]})";
        auto plan = req("POST", "/api/plans", plan_body, coach_);
        ASSERT_EQ(plan.status, 201) << plan.body;
        const long long plan_id = json_number(plan.body, "id");
        ASSERT_EQ(
            req("POST", "/api/plans/" + std::to_string(plan_id) + "/assign", "", coach_).status,
            200);
    }

    void TearDown() override {
        app_.stop();
        if (server_.joinable())
            server_.join();
    }

    std::string reg(const std::string& email, const std::string& role) {
        return fitplan::testutil::register_and_verify(port_, mail_, email, role);
    }

    long long make_exercise(const std::string& name) {
        auto r = req("POST", "/api/exercises", R"({"name":")" + name + R"("})", coach_);
        EXPECT_EQ(r.status, 201) << r.body;
        return json_number(r.body, "id");
    }

    HttpResponse req(const std::string& method, const std::string& path, const std::string& body,
                     const std::string& tok) {
        return http_request(port_, method, path, body, tok);
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
    fitplan::repositories::ExerciseNoteRepository notes_{db_.connection()};
    fitplan::services::AuthService auth_{users_, kSecret, 3600, nullptr, &ev_};
    fitplan::services::PlanService plans_{db_.connection(), plan_repo_, plan_items_, roster_,
                                          exercises_};
    fitplan::app::FitPlanApp app_;
    std::thread server_;
    std::uint16_t port_ = 0;
    std::string coach_, trainee_, other_;
    long long trainee_id_ = 0, ex_on_ = 0, ex_off_ = 0;
};

TEST_F(ExerciseNoteFlowTest, PutListGetAndDelete) {
    const std::string path = "/api/my/notes/" + std::to_string(ex_on_);

    auto created = req("PUT", path, R"({"body":"brace before the pull"})", trainee_);
    ASSERT_EQ(created.status, 200) << created.body;
    EXPECT_EQ(json_string(created.body, "exercise_name"), "Back Squat");
    EXPECT_EQ(json_string(created.body, "body"), "brace before the pull");

    // PUT again overwrites the same note
    auto updated = req("PUT", path, R"({"body":"brace, then sit back"})", trainee_);
    EXPECT_EQ(updated.status, 200);
    EXPECT_EQ(json_string(updated.body, "body"), "brace, then sit back");

    auto list = req("GET", "/api/my/notes", "", trainee_);
    EXPECT_EQ(list.status, 200);
    EXPECT_NE(list.body.find("brace, then sit back"), std::string::npos);

    // scoped to the trainee: nobody else sees it
    EXPECT_EQ(req("GET", "/api/my/notes", "", other_).body.find("brace"), std::string::npos);

    EXPECT_EQ(req("DELETE", path, "", trainee_).status, 204);
    EXPECT_EQ(req("DELETE", path, "", trainee_).status, 404);  // already gone
}

TEST_F(ExerciseNoteFlowTest, RejectsBlankBodyOffPlanAndWrongRole) {
    EXPECT_EQ(
        req("PUT", "/api/my/notes/" + std::to_string(ex_on_), R"({"body":"   "})", trainee_).status,
        400);

    // ex_off_ exists but is not on the active plan
    EXPECT_EQ(
        req("PUT", "/api/my/notes/" + std::to_string(ex_off_), R"({"body":"x"})", trainee_).status,
        404);

    // a coach has no notes endpoint access
    EXPECT_EQ(req("GET", "/api/my/notes", "", coach_).status, 403);
    EXPECT_EQ(
        req("PUT", "/api/my/notes/" + std::to_string(ex_on_), R"({"body":"x"})", coach_).status,
        403);
}

}  // namespace
