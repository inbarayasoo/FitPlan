#include "repositories/ExerciseNoteRepository.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "db/Database.hpp"

namespace {

using fitplan::db::Database;
using fitplan::repositories::ExerciseNoteRepository;

std::string migrations_dir() { return FITPLAN_TEST_MIGRATIONS_DIR; }

class ExerciseNoteRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto& c = db_.connection();
        c.exec(
            "INSERT INTO users (email, password_hash, role, display_name) "
            "VALUES ('coach@e.com','x','coach','Coach'),"
            "       ('t1@e.com','x','trainee','T1'),"
            "       ('t2@e.com','x','trainee','T2')");
        c.exec(
            "INSERT INTO exercises (coach_id, name) "
            "VALUES (1,'Back Squat'),(1,'Bench Press')");
        t1_ = 2;
        t2_ = 3;
    }

    Database db_{":memory:", migrations_dir()};
    ExerciseNoteRepository repo_{db_.connection()};
    std::int64_t t1_ = 0, t2_ = 0;
};

TEST_F(ExerciseNoteRepositoryTest, UpsertInsertsThenOverwrites) {
    const auto first = repo_.upsert(t1_, 1, "brace before the pull");
    EXPECT_GT(first.id, 0);
    EXPECT_EQ(first.body, "brace before the pull");
    EXPECT_EQ(first.exercise_name, "Back Squat");  // filled by the JOIN
    EXPECT_FALSE(first.updated_at.empty());

    const auto second = repo_.upsert(t1_, 1, "brace, then sit back");
    EXPECT_EQ(second.id, first.id);  // same row, not a new one
    EXPECT_EQ(second.body, "brace, then sit back");

    ASSERT_TRUE(repo_.find(t1_, 1).has_value());
    EXPECT_EQ(repo_.find(t1_, 1)->body, "brace, then sit back");
}

TEST_F(ExerciseNoteRepositoryTest, NotesAreScopedPerTrainee) {
    repo_.upsert(t1_, 1, "t1 note");
    repo_.upsert(t1_, 2, "t1 other note");
    repo_.upsert(t2_, 1, "t2 note");

    EXPECT_EQ(repo_.list_for_trainee(t1_).size(), 2u);
    EXPECT_EQ(repo_.list_for_trainee(t2_).size(), 1u);
    EXPECT_FALSE(repo_.find(t2_, 2).has_value());
}

TEST_F(ExerciseNoteRepositoryTest, RemoveReportsWhetherARowWasHit) {
    repo_.upsert(t1_, 1, "note");

    EXPECT_TRUE(repo_.remove(t1_, 1));
    EXPECT_FALSE(repo_.remove(t1_, 1));  // already gone
    EXPECT_FALSE(repo_.find(t1_, 1).has_value());
}

}  // namespace
