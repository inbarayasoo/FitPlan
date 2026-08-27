#include "db/Database.hpp"

#include <spdlog/spdlog.h>

#include "db/MigrationSet.hpp"

namespace fitplan::db {

Database::Database(const std::string& db_path, const std::string& migrations_dir)
    : db_(db_path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {
    enable_foreign_keys();
    apply_pending_migrations(migrations_dir);
}

void Database::enable_foreign_keys() {
    db_.exec("PRAGMA foreign_keys = ON");
}

int Database::schema_version() {
    SQLite::Statement query(db_, "SELECT COALESCE(MAX(version), 0) FROM schema_version");
    query.executeStep();
    return query.getColumn(0).getInt();
}

void Database::apply_pending_migrations(const std::string& migrations_dir) {
    db_.exec(
        "CREATE TABLE IF NOT EXISTS schema_version ("
        "  version    INTEGER PRIMARY KEY,"
        "  applied_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ")");

    const int current = schema_version();

    for (const MigrationFile& m : load_migration_files(migrations_dir)) {
        if (m.version <= current) {
            continue;
        }

        SQLite::Transaction tx(db_);
        db_.exec(m.sql);

        SQLite::Statement record(db_, "INSERT INTO schema_version(version) VALUES (?)");
        record.bind(1, m.version);
        record.exec();

        tx.commit();
        spdlog::info("Applied migration {} ({})", m.version, m.name);
    }
}

}  // namespace fitplan::db
