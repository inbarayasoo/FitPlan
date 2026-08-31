#include "db/Database.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <stdexcept>
#include <string>

namespace {

namespace fs = std::filesystem;
using fitplan::db::Database;

// Absolute path to the real migrations folder, injected by CMake so the test
// does not depend on the working directory.
std::string migrations_dir() { return FITPLAN_TEST_MIGRATIONS_DIR; }

// A throwaway on-disk database file, unique per test and deleted around it.
class DatabaseTest : public ::testing::Test {
protected:
    void SetUp() override { fs::remove(db_path_); }
    void TearDown() override { fs::remove(db_path_); }

    std::string path() const { return db_path_.string(); }

    fs::path db_path_ =
        fs::temp_directory_path() /
        (std::string("fitplan_db_test_") +
         ::testing::UnitTest::GetInstance()->current_test_info()->name() + ".db");
};

TEST_F(DatabaseTest, AppliesEveryMigration) {
    Database db(path(), migrations_dir());

    // Bump this each time a migration file is added under src/db/migrations.
    EXPECT_EQ(db.schema_version(), 3);

    SQLite::Statement tables(
        db.connection(),
        "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name IN "
        "('users', 'coach_trainees', 'exercises', 'workout_plans', 'plan_items', "
        "'workout_sessions', 'session_sets', 'exercise_notes')");
    ASSERT_TRUE(tables.executeStep());
    EXPECT_EQ(tables.getColumn(0).getInt(), 8);

    // Migration 002 added users.auth_provider with a 'local' default.
    SQLite::Statement col(
        db.connection(),
        "SELECT count(*) FROM pragma_table_info('users') WHERE name = 'auth_provider'");
    ASSERT_TRUE(col.executeStep());
    EXPECT_EQ(col.getColumn(0).getInt(), 1);
}

TEST_F(DatabaseTest, EnablesForeignKeyEnforcement) {
    Database db(path(), migrations_dir());

    SQLite::Statement q(db.connection(), "PRAGMA foreign_keys");
    ASSERT_TRUE(q.executeStep());
    EXPECT_EQ(q.getColumn(0).getInt(), 1);
}

TEST_F(DatabaseTest, IsIdempotentAcrossRestarts) {
    { Database first(path(), migrations_dir()); }

    // Opening the same file again must not try to re-run migrations that have
    // already been applied (which would fail with "table already exists" /
    // "duplicate column").
    EXPECT_NO_THROW({
        Database second(path(), migrations_dir());
        EXPECT_EQ(second.schema_version(), 3);
    });
}

TEST_F(DatabaseTest, RejectsAForeignKeyViolationOnceMigrated) {
    Database db(path(), migrations_dir());

    // exercises.coach_id references users(id); user 999 does not exist.
    EXPECT_THROW(db.connection().exec("INSERT INTO exercises (coach_id, name) "
                                     "VALUES (999, 'Orphan')"),
                 SQLite::Exception);
}

TEST_F(DatabaseTest, ThrowsWhenTheMigrationsDirectoryIsMissing) {
    EXPECT_THROW(Database(path(), "/no/such/migrations/dir"), std::runtime_error);
}

}  // namespace
