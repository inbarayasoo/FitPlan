#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <cstdint>
#include <optional>
#include <string>

#include "models/User.hpp"

namespace fitplan::repositories {

// Data access for the `users` table. Same pattern as ExerciseRepository: all SQL
// for users lives here, and it borrows the connection rather than owning it.
class UserRepository {
public:
    explicit UserRepository(SQLite::Database& db) : db_(db) {}

    // Inserts a new user (email, password_hash, role, display_name). Returns the
    // stored user with its id and created_at filled in. Throws
    // SQLite::Exception if the email is already taken (UNIQUE constraint).
    models::User create(const models::User& u);

    std::optional<models::User> find_by_id(std::int64_t id);

    // Look up by email - the key used at login. std::nullopt if no such user.
    std::optional<models::User> find_by_email(const std::string& email);

    // True if a row with this email already exists.
    bool email_exists(const std::string& email);

private:
    SQLite::Database& db_;
};

}  // namespace fitplan::repositories
