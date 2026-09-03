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
std::string migrations_dir() {
    return FITPLAN_TEST_MIGRATIONS_DIR;
}

// A throwaway on-disk database file, unique per test and deleted around it.
class DatabaseTest : public ::testing::Test {
protected:
    void SetUp() override { fs::remove(db_path_); }
    void TearDown() override { fs::remove(db_path_); }

    std::string path() const { return db_path_.string(); }

    fs::path db_path_ = fs::temp_directory_path() /
                        (std::string("fitplan_db_test_") +
                         ::testing::UnitTest::GetInstance()->current_test_info()->name() + ".db");
};

TEST_F(DatabaseTest, AppliesEveryMigration) {
    Database db(path(), migrations_dir());

    // Bump this each time a migration file is added under src/db/migrations.
    EXPECT_EQ(db.schema_version(), 6);

    SQLite::Statement tables(
        db.connection(),
        "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name IN "
        "('users', 'coach_trainees', 'exercises', 'workout_plans', 'plan_items', "
        "'workout_sessions', 'session_sets', 'exercise_notes', 'email_verification_tokens')");
    ASSERT_TRUE(tables.executeStep());
    EXPECT_EQ(tables.getColumn(0).getInt(), 9);

    // Migration 002 added users.auth_provider with a 'local' default.
    SQLite::Statement col(
        db.connection(),
        "SELECT count(*) FROM pragma_table_info('users') WHERE name = 'auth_provider'");
    ASSERT_TRUE(col.executeStep());
    EXPECT_EQ(col.getColumn(0).getInt(), 1);

    // Migration 004 rebuilt users: password_hash is now nullable and google_sub
    // was added.
    SQLite::Statement pw(
        db.connection(),
        R"(SELECT "notnull" FROM pragma_table_info('users') WHERE name = 'password_hash')");
    ASSERT_TRUE(pw.executeStep());
    EXPECT_EQ(pw.getColumn(0).getInt(), 0);

    SQLite::Statement gsub(
        db.connection(),
        "SELECT count(*) FROM pragma_table_info('users') WHERE name = 'google_sub'");
    ASSERT_TRUE(gsub.executeStep());
    EXPECT_EQ(gsub.getColumn(0).getInt(), 1);

    // Migration 005 added users.email_verified and the email_verification_tokens table.
    SQLite::Statement ev(
        db.connection(),
        "SELECT count(*) FROM pragma_table_info('users') WHERE name = 'email_verified'");
    ASSERT_TRUE(ev.executeStep());
    EXPECT_EQ(ev.getColumn(0).getInt(), 1);

    // Migration 006 moved the free-text note off the session and onto each set:
    // session_sets gained `notes`, workout_sessions lost it.
    SQLite::Statement set_notes(
        db.connection(),
        "SELECT count(*) FROM pragma_table_info('session_sets') WHERE name = 'notes'");
    ASSERT_TRUE(set_notes.executeStep());
    EXPECT_EQ(set_notes.getColumn(0).getInt(), 1);

    SQLite::Statement session_notes(
        db.connection(),
        "SELECT count(*) FROM pragma_table_info('workout_sessions') WHERE name = 'notes'");
    ASSERT_TRUE(session_notes.executeStep());
    EXPECT_EQ(session_notes.getColumn(0).getInt(), 0);
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
        EXPECT_EQ(second.schema_version(), 6);
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

// Migration 004 rebuilds the users table. This walks the realistic upgrade path
// - a database already at v3 with data in it, including a child row whose foreign
// key points at users(id) - and checks the rebuild kept every row (it must run
// with foreign keys OFF, or DROP TABLE users would cascade the child away) and
// filled the new columns with their defaults.
TEST_F(DatabaseTest, Migration004RebuildPreservesRowsAndForeignKeys) {
    const fs::path v3_dir = fs::temp_directory_path() /
                            (std::string("fitplan_pre004_") +
                             ::testing::UnitTest::GetInstance()->current_test_info()->name());
    fs::remove_all(v3_dir);
    fs::create_directories(v3_dir);
    for (const auto& entry : fs::directory_iterator(migrations_dir())) {
        const std::string name = entry.path().filename().string();
        // Everything up to and including 003; 004 is the rebuild under test and
        // 005 / 006 build on top of it.
        if (name.rfind("004_", 0) != 0 && name.rfind("005_", 0) != 0 &&
            name.rfind("006_", 0) != 0) {
            fs::copy_file(entry.path(), v3_dir / name);
        }
    }

    {
        Database db(path(), v3_dir.string());
        ASSERT_EQ(db.schema_version(), 3);
        db.connection().exec(
            "INSERT INTO users (id, email, password_hash, role, display_name) "
            "VALUES (7, 'keep@itest.com', 'hash', 'coach', 'Keep Me')");
        db.connection().exec("INSERT INTO exercises (id, coach_id, name) VALUES (1, 7, 'Squat')");
    }

    Database upgraded(path(), migrations_dir());
    EXPECT_EQ(upgraded.schema_version(), 6);

    SQLite::Statement user(
        upgraded.connection(),
        "SELECT email, auth_provider, google_sub IS NULL, email_verified FROM users WHERE id = 7");
    ASSERT_TRUE(user.executeStep());
    EXPECT_EQ(user.getColumn(0).getString(), "keep@itest.com");
    EXPECT_EQ(user.getColumn(1).getString(), "local");
    EXPECT_EQ(user.getColumn(2).getInt(), 1);  // google_sub is NULL, not ""
    EXPECT_EQ(user.getColumn(3).getInt(), 1);  // 005 grandfathers rows that predate verification

    SQLite::Statement child(upgraded.connection(),
                            "SELECT count(*) FROM exercises WHERE coach_id = 7");
    ASSERT_TRUE(child.executeStep());
    EXPECT_EQ(child.getColumn(0).getInt(), 1);

    fs::remove_all(v3_dir);
}

}  // namespace
