#include "repositories/ExerciseRepository.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "db/Database.hpp"
#include "models/Exercise.hpp"

namespace {

using fitplan::db::Database;
using fitplan::models::Exercise;
using fitplan::repositories::ExerciseRepository;

std::string migrations_dir() { return FITPLAN_TEST_MIGRATIONS_DIR; }

// Every test gets a fresh, migrated, in-memory database and one coach to own the
// exercises. ":memory:" means no files and full isolation between tests.
class ExerciseRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_.connection().exec(
            "INSERT INTO users (email, password_hash, role, display_name) "
            "VALUES ('coach@example.com', 'x', 'coach', 'Coach One')");
        coach_id_ = db_.connection().getLastInsertRowid();
    }

    // A valid exercise with one nullable field (description) left unset.
    Exercise sample() const {
        Exercise e;
        e.coach_id = coach_id_;
        e.name = "Back Squat";
        e.category = "lower";
        e.primary_muscle = "quadriceps";
        e.video_url = "https://www.youtube.com/watch?v=abcdefghijk";
        return e;
    }

    std::int64_t make_second_coach() {
        db_.connection().exec(
            "INSERT INTO users (email, password_hash, role, display_name) "
            "VALUES ('coach2@example.com', 'x', 'coach', 'Coach Two')");
        return db_.connection().getLastInsertRowid();
    }

    Database db_{":memory:", migrations_dir()};
    ExerciseRepository repo_{db_.connection()};
    std::int64_t coach_id_ = 0;
};

TEST_F(ExerciseRepositoryTest, CreateAssignsIdAndCreatedAt) {
    const Exercise saved = repo_.create(sample());

    EXPECT_GT(saved.id, 0);
    EXPECT_FALSE(saved.created_at.empty());
    EXPECT_EQ(saved.name, "Back Squat");
    EXPECT_EQ(saved.coach_id, coach_id_);
}

TEST_F(ExerciseRepositoryTest, CreateRoundTripsNullableFields) {
    const Exercise saved = repo_.create(sample());

    ASSERT_TRUE(saved.category.has_value());
    EXPECT_EQ(*saved.category, "lower");
    ASSERT_TRUE(saved.video_url.has_value());
    EXPECT_EQ(*saved.video_url, "https://www.youtube.com/watch?v=abcdefghijk");
    EXPECT_FALSE(saved.description.has_value());  // never set
}

TEST_F(ExerciseRepositoryTest, FindByIdReturnsNulloptWhenAbsent) {
    EXPECT_FALSE(repo_.find_by_id(4242).has_value());
}

TEST_F(ExerciseRepositoryTest, FindByIdReturnsTheStoredRow) {
    const Exercise saved = repo_.create(sample());

    const auto found = repo_.find_by_id(saved.id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->id, saved.id);
    EXPECT_EQ(found->name, "Back Squat");
    EXPECT_EQ(found->primary_muscle, "quadriceps");
}

TEST_F(ExerciseRepositoryTest, ListByCoachIsNewestFirst) {
    const Exercise first = repo_.create(sample());
    Exercise other = sample();
    other.name = "Deadlift";
    const Exercise second = repo_.create(other);

    const auto list = repo_.list_by_coach(coach_id_);

    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(list[0].id, second.id);  // created later, comes first
    EXPECT_EQ(list[1].id, first.id);
}

TEST_F(ExerciseRepositoryTest, ListByCoachIsScopedToOneCoach) {
    repo_.create(sample());

    Exercise other_coachs = sample();
    other_coachs.coach_id = make_second_coach();
    other_coachs.name = "Bench Press";
    repo_.create(other_coachs);

    const auto list = repo_.list_by_coach(coach_id_);

    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0].name, "Back Squat");
}

TEST_F(ExerciseRepositoryTest, UpdateChangesFieldsAndCanClearANullable) {
    Exercise e = repo_.create(sample());
    e.name = "High-Bar Back Squat";
    e.description = "Bar on the traps, upright torso.";
    e.video_url = std::nullopt;  // clear it

    EXPECT_TRUE(repo_.update(e));

    const auto got = repo_.find_by_id(e.id).value();
    EXPECT_EQ(got.name, "High-Bar Back Squat");
    ASSERT_TRUE(got.description.has_value());
    EXPECT_EQ(*got.description, "Bar on the traps, upright torso.");
    EXPECT_FALSE(got.video_url.has_value());
}

TEST_F(ExerciseRepositoryTest, UpdateReturnsFalseWhenIdDoesNotExist) {
    Exercise ghost;
    ghost.id = 99999;
    ghost.name = "Nope";
    EXPECT_FALSE(repo_.update(ghost));
}

TEST_F(ExerciseRepositoryTest, RemoveDeletesTheRow) {
    const Exercise saved = repo_.create(sample());

    EXPECT_TRUE(repo_.remove(saved.id));
    EXPECT_FALSE(repo_.find_by_id(saved.id).has_value());
}

TEST_F(ExerciseRepositoryTest, RemoveReturnsFalseWhenIdDoesNotExist) {
    EXPECT_FALSE(repo_.remove(12345));
}

}  // namespace
