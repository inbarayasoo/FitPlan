#include "repositories/SessionRepository.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "db/Database.hpp"
#include "models/SessionSet.hpp"
#include "models/WorkoutSession.hpp"
#include "repositories/SessionSetRepository.hpp"

namespace {

using fitplan::db::Database;
using fitplan::models::SessionSet;
using fitplan::models::WorkoutSession;
using fitplan::repositories::SessionRepository;
using fitplan::repositories::SessionSetRepository;

std::string migrations_dir() {
    return FITPLAN_TEST_MIGRATIONS_DIR;
}

class SessionRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& c = db_.connection();
        c.exec(
            "INSERT INTO users (email, password_hash, role, display_name) "
            "VALUES ('coach@example.com','x','coach','Coach'),"
            "       ('trainee@example.com','x','trainee','Trainee')");
        c.exec("INSERT INTO exercises (coach_id, name) VALUES (1, 'Back Squat')");
        c.exec(
            "INSERT INTO workout_plans (coach_id, trainee_id, name) "
            "VALUES (1, 2, 'Week 1')");
        c.exec(
            "INSERT INTO plan_items (plan_id, exercise_id, order_index) "
            "VALUES (1, 1, 0)");
        trainee_id_ = 2;
    }

    WorkoutSession sample() const {
        WorkoutSession s;
        s.trainee_id = trainee_id_;
        s.plan_id = 1;
        s.status = "completed";
        s.notes = "felt good";
        return s;
    }

    Database db_{":memory:", migrations_dir()};
    SessionRepository sessions_{db_.connection()};
    SessionSetRepository sets_{db_.connection()};
    std::int64_t trainee_id_ = 0;
};

TEST_F(SessionRepositoryTest, CreateFillsIdAndDefaultsPerformedAt) {
    const WorkoutSession saved = sessions_.create(sample());

    EXPECT_GT(saved.id, 0);
    EXPECT_FALSE(saved.performed_at.empty());  // column default applied
    ASSERT_TRUE(saved.plan_id.has_value());
    EXPECT_EQ(*saved.plan_id, 1);
    EXPECT_EQ(saved.status, "completed");
}

TEST_F(SessionRepositoryTest, CreateHonoursAnExplicitPerformedAt) {
    WorkoutSession s = sample();
    s.performed_at = "2026-08-10 09:30:00";

    const WorkoutSession saved = sessions_.create(s);
    EXPECT_EQ(saved.performed_at, "2026-08-10 09:30:00");
}

TEST_F(SessionRepositoryTest, ListByTraineeIsNewestFirst) {
    WorkoutSession older = sample();
    older.performed_at = "2026-08-01 08:00:00";
    sessions_.create(older);

    WorkoutSession newer = sample();
    newer.performed_at = "2026-08-20 08:00:00";
    sessions_.create(newer);

    const auto list = sessions_.list_by_trainee(trainee_id_);
    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(list[0].performed_at, "2026-08-20 08:00:00");
    EXPECT_EQ(list[1].performed_at, "2026-08-01 08:00:00");
}

TEST_F(SessionRepositoryTest, UpdateChangesStatusAndNotesOnly) {
    WorkoutSession saved = sessions_.create(sample());
    saved.status = "in_progress";
    saved.notes = std::nullopt;

    EXPECT_TRUE(sessions_.update(saved));
    const auto reloaded = sessions_.find_by_id(saved.id);
    ASSERT_TRUE(reloaded.has_value());
    EXPECT_EQ(reloaded->status, "in_progress");
    EXPECT_FALSE(reloaded->notes.has_value());
}

TEST_F(SessionRepositoryTest, SessionSetsRoundTripAndDeleteBySession) {
    const WorkoutSession session = sessions_.create(sample());

    SessionSet set;
    set.session_id = session.id;
    set.exercise_id = 1;
    set.plan_item_id = 1;
    set.set_number = 1;
    set.reps = 5;
    set.weight = 80.0;
    set.rpe = 7.5;
    const SessionSet saved = sets_.create(set);

    EXPECT_GT(saved.id, 0);
    ASSERT_TRUE(saved.reps.has_value());
    EXPECT_EQ(*saved.reps, 5);
    EXPECT_TRUE(saved.completed);  // column default 1

    set.set_number = 2;
    sets_.create(set);
    EXPECT_EQ(sets_.list_by_session(session.id).size(), 2u);
    EXPECT_EQ(sets_.delete_by_session(session.id), 2);
    EXPECT_TRUE(sets_.list_by_session(session.id).empty());

    EXPECT_EQ(saved.exercise_name, "Back Squat");  // filled by the JOIN
}

TEST_F(SessionRepositoryTest, RemoveDeletesTheSessionAndCascadesItsSets) {
    const WorkoutSession session = sessions_.create(sample());
    SessionSet set;
    set.session_id = session.id;
    set.exercise_id = 1;
    set.set_number = 1;
    sets_.create(set);

    EXPECT_TRUE(sessions_.remove(session.id));
    EXPECT_FALSE(sessions_.find_by_id(session.id).has_value());
    EXPECT_TRUE(sets_.list_by_session(session.id).empty());  // ON DELETE CASCADE
    EXPECT_FALSE(sessions_.remove(session.id));              // already gone
}

}  // namespace
