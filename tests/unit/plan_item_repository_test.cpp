#include "repositories/PlanItemRepository.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "db/Database.hpp"
#include "models/PlanItem.hpp"

namespace {

using fitplan::db::Database;
using fitplan::models::PlanItem;
using fitplan::repositories::PlanItemRepository;

std::string migrations_dir() {
    return FITPLAN_TEST_MIGRATIONS_DIR;
}

class PlanItemRepositoryTest : public ::testing::Test {
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
        plan_id_ = 1;
        exercise_id_ = 1;
    }

    PlanItem sample() const {
        PlanItem it;
        it.plan_id = plan_id_;
        it.exercise_id = exercise_id_;
        it.order_index = 0;
        it.target_sets = 5;
        it.target_reps = 5;
        it.target_weight = 100.0;
        it.rest_seconds = 180;
        it.day_label = "A";
        return it;
    }

    Database db_{":memory:", migrations_dir()};
    PlanItemRepository repo_{db_.connection()};
    std::int64_t plan_id_ = 0;
    std::int64_t exercise_id_ = 0;
};

TEST_F(PlanItemRepositoryTest, CreateRoundTripsSetAndUnsetOptionals) {
    const PlanItem saved = repo_.create(sample());

    EXPECT_GT(saved.id, 0);
    ASSERT_TRUE(saved.target_weight.has_value());
    EXPECT_DOUBLE_EQ(*saved.target_weight, 100.0);
    ASSERT_TRUE(saved.day_label.has_value());
    EXPECT_EQ(*saved.day_label, "A");
    EXPECT_FALSE(saved.notes.has_value());      // never set
    EXPECT_FALSE(saved.video_url.has_value());  // never set
}

TEST_F(PlanItemRepositoryTest, ListByPlanIsOrderedByOrderIndex) {
    PlanItem second = sample();
    second.order_index = 2;
    second.day_label = "C";
    repo_.create(second);

    PlanItem first = sample();
    first.order_index = 1;
    first.day_label = "B";
    repo_.create(first);

    PlanItem zeroth = sample();
    zeroth.order_index = 0;
    zeroth.day_label = "A";
    repo_.create(zeroth);

    const auto items = repo_.list_by_plan(plan_id_);
    ASSERT_EQ(items.size(), 3u);
    EXPECT_EQ(*items[0].day_label, "A");
    EXPECT_EQ(*items[1].day_label, "B");
    EXPECT_EQ(*items[2].day_label, "C");
}

TEST_F(PlanItemRepositoryTest, DeleteByPlanRemovesEveryItemAndReportsCount) {
    repo_.create(sample());
    repo_.create(sample());
    repo_.create(sample());

    EXPECT_EQ(repo_.delete_by_plan(plan_id_), 3);
    EXPECT_TRUE(repo_.list_by_plan(plan_id_).empty());
}

}  // namespace
