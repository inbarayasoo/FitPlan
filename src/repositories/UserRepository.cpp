#include "repositories/UserRepository.hpp"

#include <string>

namespace fitplan::repositories {

namespace {

constexpr const char* kSelectColumns = "id, email, password_hash, role, display_name, created_at";

// Column order must match kSelectColumns:
//   0 id   1 email   2 password_hash   3 role   4 display_name   5 created_at
models::User row_to_user(SQLite::Statement& stmt) {
    models::User u;
    u.id = stmt.getColumn(0).getInt64();
    u.email = stmt.getColumn(1).getString();
    u.password_hash = stmt.getColumn(2).getString();
    u.role = stmt.getColumn(3).getString();
    u.display_name = stmt.getColumn(4).getString();
    u.created_at = stmt.getColumn(5).getString();
    return u;
}

}  // namespace

models::User UserRepository::create(const models::User& u) {
    SQLite::Statement stmt(db_,
                           "INSERT INTO users (email, password_hash, role, display_name) "
                           "VALUES (?, ?, ?, ?)");
    stmt.bind(1, u.email);
    stmt.bind(2, u.password_hash);
    stmt.bind(3, u.role);
    stmt.bind(4, u.display_name);
    stmt.exec();

    return find_by_id(db_.getLastInsertRowid()).value();
}

std::optional<models::User> UserRepository::find_by_id(std::int64_t id) {
    SQLite::Statement stmt(db_,
                           std::string("SELECT ") + kSelectColumns + " FROM users WHERE id = ?");
    stmt.bind(1, id);
    if (!stmt.executeStep()) {
        return std::nullopt;
    }
    return row_to_user(stmt);
}

std::optional<models::User> UserRepository::find_by_email(const std::string& email) {
    SQLite::Statement stmt(db_,
                           std::string("SELECT ") + kSelectColumns + " FROM users WHERE email = ?");
    stmt.bind(1, email);
    if (!stmt.executeStep()) {
        return std::nullopt;
    }
    return row_to_user(stmt);
}

bool UserRepository::email_exists(const std::string& email) {
    SQLite::Statement stmt(db_, "SELECT 1 FROM users WHERE email = ? LIMIT 1");
    stmt.bind(1, email);
    return stmt.executeStep();
}

}  // namespace fitplan::repositories
