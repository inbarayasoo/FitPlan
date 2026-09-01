#include "services/SessionService.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "db/Database.hpp"
#include "repositories/ExerciseRepository.hpp"
#include "repositories/PlanItemRepository.hpp"
#include "repositories/PlanRepository.hpp"
#include "repositories/SessionRepository.hpp"
#include "repositories/SessionSetRepository.hpp"
#include "services/SessionError.hpp"

namespace {

using fitplan::db::Database;
using fitplan::services::SessionError;
using fitplan::services::SessionErrorKind;
using fitplan::services::SessionInput;
using fitplan::services::SessionPatch;
using fitplan::services::SessionService;
using fitplan::services::SessionSetInput;

std::string migrations_dir() {
    return FITPLAN_TEST_MIGRATIONS_DIR;
}

class SessionServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& c = db_.connection();
        c.exec(
            "INSERT INTO users (email, password_hash, role, display_name) VALUES "
            "('coach@x.com','x','coach','Coach'),"
            "('t1@x.com','x','trainee','T1'),"
            "('t2@x.com','x','trainee','T2')");
        c.exec(
            "INSERT INTO exercises (coach_id, name, video_url) VALUES "
            "(1,'Back Squat','https://www.youtube.com/watch?v=dQw4w9WgXcQ'),"
            "(1,'Bench Press',NULL)");
        c.exec(
            "INSERT INTO workout_plans (id, coach_id, trainee_id, name, is_active) "
            "VALUES (1,1,2,'Week 1',1)");
        c.exec(
            "INSERT INTO plan_items (id, plan_id, exercise_id, order_index, "
            "target_sets, video_url) VALUES "
            "(1,1,1,0,3,NULL),"
            "(2,1,2,1,2,'https://youtu.be/abcdef12345')");
        t1_ = 2;
        t2_ = 3;
    }

    SessionInput one_set_session() const {
        SessionSetInput s;
        s.exercise_id = 1;
        s.plan_item_id = 1;
        s.reps = 5;
        s.weight = 80.0;
        SessionInput in;
        in.plan_id = 1;
        in.sets = {s, s, s};
        return in;
    }

    Database db_{":memory:", migrations_dir()};
    fitplan::repositories::SessionRepository sessions_{db_.connection()};
    fitplan::repositories::SessionSetRepository session_sets_{db_.connection()};
    fitplan::repositories::PlanRepository plans_{db_.connection()};
    fitplan::repositories::PlanItemRepository plan_items_{db_.connection()};
    fitplan::repositories::ExerciseRepository exercises_{db_.connection()};
    SessionService svc_{db_.connection(), sessions_,   session_sets_,
                        plans_,           plan_items_, exercises_};
    std::int64_t t1_ = 0, t2_ = 0;
};

TEST_F(SessionServiceTest, LogSessionStoresHeaderAndNumbersSets) {
    const auto out = svc_.log_session(t1_, one_set_session());

    EXPECT_GT(out.session.id, 0);
    EXPECT_EQ(out.session.status, "completed");
    ASSERT_EQ(out.sets.size(), 3u);
    EXPECT_EQ(out.sets[0].set_number, 1);
    EXPECT_EQ(out.sets[1].set_number, 2);
    EXPECT_EQ(out.sets[2].set_number, 3);
}

TEST_F(SessionServiceTest, LogSessionRejectsBadStatus) {
    SessionInput in = one_set_session();
    in.status = "done";
    try {
        svc_.log_session(t1_, in);
        FAIL() << "expected SessionError";
    } catch (const SessionError& e) {
        EXPECT_EQ(e.kind(), SessionErrorKind::kInvalidInput);
    }
}

TEST_F(SessionServiceTest, LogSessionRejectsUnknownExerciseAndForeignPlanItem) {
    SessionInput unknown_ex = one_set_session();
    unknown_ex.sets = {SessionSetInput{}};
    unknown_ex.sets[0].exercise_id = 999;
    EXPECT_THROW(svc_.log_session(t1_, unknown_ex), SessionError);

    SessionInput foreign_item = one_set_session();
    foreign_item.sets = {SessionSetInput{}};
    foreign_item.sets[0].exercise_id = 1;
    foreign_item.sets[0].plan_item_id = 42;  // not on t1's plan
    try {
        svc_.log_session(t1_, foreign_item);
        FAIL() << "expected SessionError";
    } catch (const SessionError& e) {
        EXPECT_EQ(e.kind(), SessionErrorKind::kForbidden);
    }
}

TEST_F(SessionServiceTest, ActivePlanForFillsEffectiveVideoUrl) {
    const auto plan = svc_.active_plan_for(t1_);
    ASSERT_EQ(plan.items.size(), 2u);
    // item 1 had no override -> inherits the exercise's library link
    ASSERT_TRUE(plan.items[0].video_url.has_value());
    EXPECT_NE(plan.items[0].video_url->find("youtube.com"), std::string::npos);
    // item 2 keeps its own override
    ASSERT_TRUE(plan.items[1].video_url.has_value());
    EXPECT_NE(plan.items[1].video_url->find("youtu.be"), std::string::npos);
}

TEST_F(SessionServiceTest, ActivePlanForThrowsWhenNoneActive) {
    try {
        svc_.active_plan_for(t2_);  // t2 has no plan
        FAIL() << "expected SessionError";
    } catch (const SessionError& e) {
        EXPECT_EQ(e.kind(), SessionErrorKind::kNotFound);
    }
}

TEST_F(SessionServiceTest, UpdateSessionChangesStatusAndIsTraineeScoped) {
    const auto out = svc_.log_session(t1_, one_set_session());

    SessionPatch patch;
    patch.status = "in_progress";
    const auto updated = svc_.update_session(t1_, out.session.id, patch);
    EXPECT_EQ(updated.session.status, "in_progress");

    EXPECT_THROW(svc_.update_session(t2_, out.session.id, patch), SessionError);
}

TEST_F(SessionServiceTest, FlattenedInputsForProgress) {
    svc_.log_session(t1_, one_set_session());

    const auto sets = svc_.logged_sets_for(t1_);
    ASSERT_EQ(sets.size(), 3u);
    EXPECT_EQ(sets[0].exercise_id, 1);
    EXPECT_EQ(sets[0].performed_on.size(), 10u);  // "YYYY-MM-DD"
    ASSERT_TRUE(sets[0].plan_item_id.has_value());
    EXPECT_EQ(*sets[0].plan_item_id, 1);

    const auto prescribed = svc_.prescribed_for(t1_);
    ASSERT_EQ(prescribed.size(), 2u);
    EXPECT_EQ(prescribed[0].target_sets, 3);
    EXPECT_EQ(prescribed[1].target_sets, 2);
}

}  // namespace
