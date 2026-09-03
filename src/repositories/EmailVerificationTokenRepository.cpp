#include "repositories/EmailVerificationTokenRepository.hpp"

#include <string>

namespace fitplan::repositories {

namespace {

constexpr const char* kSelectColumns = "id, user_id, code_hash, expires_at, attempts, issued_at";

// Column order must match kSelectColumns:
//   0 id  1 user_id  2 code_hash  3 expires_at  4 attempts  5 issued_at
models::EmailVerificationToken row_to_token(SQLite::Statement& stmt) {
    models::EmailVerificationToken t;
    t.id = stmt.getColumn(0).getInt64();
    t.user_id = stmt.getColumn(1).getInt64();
    t.code_hash = stmt.getColumn(2).getString();
    t.expires_at = stmt.getColumn(3).getString();
    t.attempts = stmt.getColumn(4).getInt();
    t.issued_at = stmt.getColumn(5).getString();
    return t;
}

}  // namespace

models::EmailVerificationToken EmailVerificationTokenRepository::upsert(
    std::int64_t user_id, const std::string& code_hash, const std::string& expires_at,
    const std::string& issued_at) {
    SQLite::Statement stmt(
        db_,
        "INSERT INTO email_verification_tokens (user_id, code_hash, expires_at, issued_at) "
        "VALUES (?, ?, ?, ?) "
        "ON CONFLICT (user_id) DO UPDATE SET "
        "  code_hash = excluded.code_hash, "
        "  expires_at = excluded.expires_at, "
        "  attempts = 0, "
        "  issued_at = excluded.issued_at");
    stmt.bind(1, user_id);
    stmt.bind(2, code_hash);
    stmt.bind(3, expires_at);
    stmt.bind(4, issued_at);
    stmt.exec();
    return find_for_user(user_id).value();
}

std::optional<models::EmailVerificationToken> EmailVerificationTokenRepository::find_for_user(
    std::int64_t user_id) {
    SQLite::Statement stmt(db_, std::string("SELECT ") + kSelectColumns +
                                    " FROM email_verification_tokens WHERE user_id = ?");
    stmt.bind(1, user_id);
    if (!stmt.executeStep()) {
        return std::nullopt;
    }
    return row_to_token(stmt);
}

void EmailVerificationTokenRepository::increment_attempts(std::int64_t user_id) {
    SQLite::Statement stmt(
        db_, "UPDATE email_verification_tokens SET attempts = attempts + 1 WHERE user_id = ?");
    stmt.bind(1, user_id);
    stmt.exec();
}

bool EmailVerificationTokenRepository::delete_for_user(std::int64_t user_id) {
    SQLite::Statement stmt(db_, "DELETE FROM email_verification_tokens WHERE user_id = ?");
    stmt.bind(1, user_id);
    return stmt.exec() > 0;
}

}  // namespace fitplan::repositories
