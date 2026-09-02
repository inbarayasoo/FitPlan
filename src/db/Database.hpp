#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <string>

namespace fitplan::db {

// Owns the process-wide SQLite connection and brings the schema up to date.
//
// RAII: the connection is opened by the constructor and closed automatically
// when the Database object is destroyed - there is no close() to remember and
// no way to leak the handle if an exception is thrown.
class Database {
public:
    // Opens the SQLite file at `db_path` (creating it if it does not exist),
    // enables foreign-key enforcement, then applies every migration in
    // `migrations_dir` that has not run yet, in ascending version order.
    Database(const std::string& db_path, const std::string& migrations_dir);

    // The underlying connection, handed to repositories so they can run queries.
    SQLite::Database& connection() { return db_; }

    // Highest migration version recorded in schema_version, or 0 if none.
    int schema_version();

private:
    void enable_foreign_keys();
    void apply_pending_migrations(const std::string& migrations_dir);
    void record_migration(int version);

    SQLite::Database db_;
};

}  // namespace fitplan::db
