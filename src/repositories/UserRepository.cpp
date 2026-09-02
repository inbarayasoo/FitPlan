#include "repositories/UserRepository.hpp"

#include <string>

namespace fitplan::repositories {

namespace {

constexpr const char* kSelectColumns =
    "id, email, password_hash, role, display_name, created_at, auth_provider, google_sub";

// Column order must match kSelectColumns:
//   0 id   1 email   2 password_hash   3 role   4 display_name
//   5 created_at   6 auth_provider   7 google_sub
models::User row_to_user(SQLite::Statement& stmt) {
    models::User u;
    u.id = stmt.getColumn(0).getInt64();
    u.email = stmt.getColumn(1).getString();
    u.password_hash = stmt.getColumn(2).getString();  // "" when the column is NULL
    u.role = stmt.getColumn(3).getString();
    u.display_name = stmt.getColumn(4).getString();
    u.created_at = stmt.getColumn(5).getString();
    u.auth_provider = stmt.getColumn(6).getString();
    u.google_sub = stmt.getColumn(7).getString();  // "" when the column is NULL
    return u;
}

// Binds a TEXT parameter, or SQL NULL when the string is empty. NULL - not "" -
// is what an absent password or Google id must be: two NULLs never collide under
// a UNIQUE index, two empty strings do.
void bind_text_or_null(SQLite::Statement& stmt, int index, const std::string& value) {
    if (value.empty()) {
        stmt.bind(index);  // one argument => bind SQL NULL
    } else {
        stmt.bind(index, value);
    }
}

}  // namespace

models::User UserRepository::create(const models::User& u) {
    SQLite::Statement stmt(db_,
                           "INSERT INTO users "
                           "(email, password_hash, role, display_name, auth_provider, google_sub) "
                           "VALUES (?, ?, ?, ?, ?, ?)");
    stmt.bind(1, u.email);
    bind_text_or_null(stmt, 2, u.password_hash);
    stmt.bind(3, u.role);
    stmt.bind(4, u.display_name);
    stmt.bind(5, u.auth_provider.empty() ? std::string("local") : u.auth_provider);
    bind_text_or_null(stmt, 6, u.google_sub);
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

std::optional<models::User> UserRepository::find_by_google_sub(const std::string& google_sub) {
    SQLite::Statement stmt(
        db_, std::string("SELECT ") + kSelectColumns + " FROM users WHERE google_sub = ?");
    stmt.bind(1, google_sub);
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

void UserRepository::link_google(std::int64_t user_id, const std::string& google_sub) {
    SQLite::Statement stmt(db_, "UPDATE users SET google_sub = ? WHERE id = ?");
    stmt.bind(1, google_sub);
    stmt.bind(2, user_id);
    stmt.exec();
}

}  // namespace fitplan::repositories
