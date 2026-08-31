#include "repositories/CoachTraineeRepository.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "db/Database.hpp"

namespace {

using fitplan::db::Database;
using fitplan::repositories::CoachTraineeRepository;

std::string migrations_dir() { return FITPLAN_TEST_MIGRATIONS_DIR; }

class CoachTraineeRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        coach_a_ = insert_user("coachA@example.com", "coach", "Coach A");
        coach_b_ = insert_user("coachB@example.com", "coach", "Coach B");
        trainee_1_ = insert_user("t1@example.com", "trainee", "Trainee One");
        trainee_2_ = insert_user("t2@example.com", "trainee", "Trainee Two");
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

    Database db_{":memory:", migrations_dir()};
    CoachTraineeRepository repo_{db_.connection()};
    std::int64_t coach_a_ = 0, coach_b_ = 0, trainee_1_ = 0, trainee_2_ = 0;
};

TEST_F(CoachTraineeRepositoryTest, LinkIsIdempotentAndReported) {
    EXPECT_TRUE(repo_.link(coach_a_, trainee_1_));   // new
    EXPECT_FALSE(repo_.link(coach_a_, trainee_1_));  // already linked
    EXPECT_TRUE(repo_.is_linked(coach_a_, trainee_1_));
    EXPECT_FALSE(repo_.is_linked(coach_b_, trainee_1_));
}

TEST_F(CoachTraineeRepositoryTest, ListTraineesIsScopedAndNewestFirst) {
    repo_.link(coach_a_, trainee_1_);
    repo_.link(coach_a_, trainee_2_);
    repo_.link(coach_b_, trainee_1_);

    const auto roster = repo_.list_trainees(coach_a_);
    ASSERT_EQ(roster.size(), 2u);
    EXPECT_EQ(roster[0].id, trainee_2_);  // linked last, listed first
    EXPECT_EQ(roster[1].id, trainee_1_);
    EXPECT_EQ(roster[0].email, "t2@example.com");

    EXPECT_EQ(repo_.list_trainees(coach_b_).size(), 1u);
}

TEST_F(CoachTraineeRepositoryTest, UnlinkRemovesOnlyThatPair) {
    repo_.link(coach_a_, trainee_1_);
    repo_.link(coach_a_, trainee_2_);

    EXPECT_TRUE(repo_.unlink(coach_a_, trainee_1_));   // link existed, removed
    EXPECT_FALSE(repo_.unlink(coach_a_, trainee_1_));  // nothing left to remove
    EXPECT_FALSE(repo_.is_linked(coach_a_, trainee_1_));
    EXPECT_TRUE(repo_.is_linked(coach_a_, trainee_2_));  // the other pair stands
}

}  // namespace
