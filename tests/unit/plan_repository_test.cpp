#include "repositories/PlanRepository.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "db/Database.hpp"
#include "models/WorkoutPlan.hpp"

namespace {

using fitplan::db::Database;
using fitplan::models::WorkoutPlan;
using fitplan::repositories::PlanRepository;

std::string migrations_dir() { return FITPLAN_TEST_MIGRATIONS_DIR; }

class PlanRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        coach_id_ = insert_user("coach@example.com", "coach", "Coach One");
        trainee_id_ = insert_user("trainee@example.com", "trainee", "Trainee One");
    }

    std::int64_t insert_user(const std::string& email, const std::string& role,
                             const std::string& name) {
        SQLite::Statement stmt(
            db_.connection(),
            "INSERT INTO users (email, password_hash, role, display_name) "
            "VALUES (?, 'x', ?, ?)");
        stmt.bind(1, email);
        stmt.bind(2, role);
        stmt.bind(3, name);
        stmt.exec();
        return db_.connection().getLastInsertRowid();
    }

    WorkoutPlan sample() const {
        WorkoutPlan p;
        p.coach_id = coach_id_;
        p.trainee_id = trainee_id_;
        p.name = "Week 1 - Full Body";
        p.notes = "3 sessions, alternate A/B";
        return p;
    }

    Database db_{":memory:", migrations_dir()};
    PlanRepository repo_{db_.connection()};
    std::int64_t coach_id_ = 0;
    std::int64_t trainee_id_ = 0;
};

TEST_F(PlanRepositoryTest, CreateAssignsIdAndDefaultsToInactive) {
    const WorkoutPlan saved = repo_.create(sample());

    EXPECT_GT(saved.id, 0);
    EXPECT_FALSE(saved.created_at.empty());
    EXPECT_EQ(saved.coach_id, coach_id_);
    EXPECT_EQ(saved.trainee_id, trainee_id_);
    EXPECT_FALSE(saved.is_active);
}

TEST_F(PlanRepositoryTest, FindByIdRoundTripsNotesAndReturnsNulloptWhenAbsent) {
    EXPECT_FALSE(repo_.find_by_id(9999).has_value());

    const WorkoutPlan saved = repo_.create(sample());
    const auto found = repo_.find_by_id(saved.id);
    ASSERT_TRUE(found.has_value());
    ASSERT_TRUE(found->notes.has_value());
    EXPECT_EQ(*found->notes, "3 sessions, alternate A/B");
}

TEST_F(PlanRepositoryTest, ListByCoachIsNewestFirstAndScoped) {
    const WorkoutPlan first = repo_.create(sample());
    WorkoutPlan other = sample();
    other.name = "Week 2";
    const WorkoutPlan second = repo_.create(other);

    const std::int64_t other_coach = insert_user("c2@example.com", "coach", "C2");
    WorkoutPlan alien = sample();
    alien.coach_id = other_coach;
    repo_.create(alien);

    const auto list = repo_.list_by_coach(coach_id_);
    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(list[0].id, second.id);
    EXPECT_EQ(list[1].id, first.id);
}

TEST_F(PlanRepositoryTest, UpdateChangesNameAndNotesOnly) {
    WorkoutPlan p = repo_.create(sample());
    p.name = "Week 1 - Full Body (revised)";
    p.notes = std::nullopt;

    EXPECT_TRUE(repo_.update(p));

    const auto got = repo_.find_by_id(p.id).value();
    EXPECT_EQ(got.name, "Week 1 - Full Body (revised)");
    EXPECT_FALSE(got.notes.has_value());
}

TEST_F(PlanRepositoryTest, SetActiveAndDeactivateAllForTrainee) {
    const WorkoutPlan a = repo_.create(sample());
    const WorkoutPlan b = repo_.create(sample());

    EXPECT_TRUE(repo_.set_active(a.id, true));
    EXPECT_TRUE(repo_.find_by_id(a.id)->is_active);

    // Activating b should be paired by the caller with deactivating the rest.
    const int cleared = repo_.deactivate_all_for_trainee(trainee_id_);
    EXPECT_EQ(cleared, 1);  // only a was active
    repo_.set_active(b.id, true);

    EXPECT_FALSE(repo_.find_by_id(a.id)->is_active);
    EXPECT_TRUE(repo_.find_by_id(b.id)->is_active);
}

TEST_F(PlanRepositoryTest, RemoveDeletesTheRowAndReportsWhetherOneWasHit) {
    const WorkoutPlan p = repo_.create(sample());

    EXPECT_TRUE(repo_.remove(p.id));
    EXPECT_FALSE(repo_.find_by_id(p.id).has_value());
    EXPECT_FALSE(repo_.remove(p.id));  // already gone
}

}  // namespace
