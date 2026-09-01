#include "db/MigrationSet.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

namespace fs = std::filesystem;
using fitplan::db::load_migration_files;

// A throwaway directory that exists only for the duration of one test. write()
// drops a file into it with the given name and contents.
class MigrationDir : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = fs::temp_directory_path() /
               (std::string("fitplan_migrations_") +
                ::testing::UnitTest::GetInstance()->current_test_info()->name());
        fs::remove_all(dir_);
        fs::create_directories(dir_);
    }

    void TearDown() override { fs::remove_all(dir_); }

    void write(const std::string& name, const std::string& contents = "SELECT 1;") {
        std::ofstream(dir_ / name) << contents;
    }

    std::string dir() const { return dir_.string(); }

    fs::path dir_;
};

// The prefix is a number, so ordering is numeric: "10" comes after "2", not
// before it the way a plain string sort would put it.
TEST_F(MigrationDir, SortsByNumberNotLexicographically) {
    write("2_second.sql");
    write("10_tenth.sql");
    write("1_first.sql");
    write("21_twenty_first.sql");

    const auto files = load_migration_files(dir());

    ASSERT_EQ(files.size(), 4u);
    EXPECT_EQ(files[0].version, 1);
    EXPECT_EQ(files[1].version, 2);
    EXPECT_EQ(files[2].version, 10);
    EXPECT_EQ(files[3].version, 21);
}

// A zero-padded prefix still parses to its plain integer value, and the file's
// name and body are carried through unchanged.
TEST_F(MigrationDir, ReadsVersionNameAndBodyOfEachFile) {
    write("001_initial.sql", "CREATE TABLE t (id INTEGER);");

    const auto files = load_migration_files(dir());

    ASSERT_EQ(files.size(), 1u);
    EXPECT_EQ(files[0].version, 1);
    EXPECT_EQ(files[0].name, "001_initial.sql");
    EXPECT_EQ(files[0].sql, "CREATE TABLE t (id INTEGER);");
}

// Only files that are both ".sql" and prefixed with a number count. Everything
// else in the folder is skipped, not an error.
TEST_F(MigrationDir, IgnoresFilesThatAreNotNumberedSqlMigrations) {
    write("005_real.sql");
    write("notes.sql");           // .sql, but no leading number
    write("003_backup.sql.bak");  // leading number, but not a .sql extension
    write("README.md");
    write("draft_007.sql");  // the number is not at the start

    const auto files = load_migration_files(dir());

    ASSERT_EQ(files.size(), 1u);
    EXPECT_EQ(files[0].version, 5);
    EXPECT_EQ(files[0].name, "005_real.sql");
}

// Two files that resolve to the same version number are a mistake the loader
// must refuse, so a later migration can never be silently dropped.
TEST_F(MigrationDir, ThrowsWhenTwoFilesShareAVersionNumber) {
    write("004_alpha.sql");
    write("4_beta.sql");

    EXPECT_THROW(load_migration_files(dir()), std::runtime_error);
}

// A path that is not an existing directory is a configuration error, reported
// by throwing rather than by quietly running zero migrations.
TEST_F(MigrationDir, ThrowsWhenTheDirectoryDoesNotExist) {
    EXPECT_THROW(load_migration_files((dir_ / "does-not-exist").string()), std::runtime_error);
}

// A real directory that simply holds nothing the loader recognizes yields an
// empty list, not an exception.
TEST_F(MigrationDir, ReturnsEmptyWhenNoFileIsANumberedSqlMigration) {
    write("readme.txt", "not a migration");
    write("schema-notes.sql");  // .sql, but no leading number

    const auto files = load_migration_files(dir());

    EXPECT_TRUE(files.empty());
}

}  // namespace
