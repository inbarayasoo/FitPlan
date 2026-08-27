#pragma once

#include <string>
#include <vector>

namespace fitplan::db {

// One migration file on disk, already read into memory.
struct MigrationFile {
    int version;       // the number the filename starts with: 1, 2, 3, ...
    std::string name;  // e.g. "001_initial_schema.sql"
    std::string sql;   // the full contents of the file
};

// Loads every "*.sql" file in `dir` whose name begins with a number, sorted by
// that number ascending. Files that do not start with a number are ignored.
// Throws std::runtime_error if `dir` is missing or two files share a version.
std::vector<MigrationFile> load_migration_files(const std::string& dir);

}  // namespace fitplan::db
