#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <cstdint>
#include <optional>
#include <string>

#include "models/EmailVerificationToken.hpp"

namespace fitplan::repositories {

// Data access for `email_verification_tokens`. Same shape as the other
// repositories: all SQL for the table lives here, the connection is borrowed.
// Time and attempt policy is the service's job - this class only stores and
// returns rows.
class EmailVerificationTokenRepository {
public:
    explicit EmailVerificationTokenRepository(SQLite::Database& db) : db_(db) {}

    // Stores the pending code for this user, replacing any existing one (user_id
    // is UNIQUE): code_hash, expires_at and issued_at are overwritten and
    // attempts resets to 0. All timestamps are UTC ISO-8601 strings the caller
    // has already computed from its clock. Returns the stored row.
    models::EmailVerificationToken upsert(std::int64_t user_id, const std::string& code_hash,
                                          const std::string& expires_at,
                                          const std::string& issued_at);

    // The pending code for this user, or nullopt if there is none. Whether it has
    // expired is the caller's to decide from `expires_at`.
    std::optional<models::EmailVerificationToken> find_for_user(std::int64_t user_id);

    // Records one more failed guess. A no-op if the user has no pending code.
    void increment_attempts(std::int64_t user_id);

    // Drops this user's pending code. Returns true if a row was removed.
    bool delete_for_user(std::int64_t user_id);

private:
    SQLite::Database& db_;
};

}  // namespace fitplan::repositories
