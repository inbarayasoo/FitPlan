#include "repositories/SessionRepository.hpp"

#include <string>

namespace fitplan::repositories {

namespace {

constexpr const char* kSelectColumns = "id, trainee_id, plan_id, performed_at, status";

// Column order must match kSelectColumns:
//   0 id  1 trainee_id  2 plan_id  3 performed_at  4 status
models::WorkoutSession row_to_session(SQLite::Statement& stmt) {
    models::WorkoutSession s;
    s.id = stmt.getColumn(0).getInt64();
    s.trainee_id = stmt.getColumn(1).getInt64();
    if (!stmt.getColumn(2).isNull())
        s.plan_id = stmt.getColumn(2).getInt64();
    s.performed_at = stmt.getColumn(3).getString();
    s.status = stmt.getColumn(4).getString();
    return s;
}

void bind_optional_id(SQLite::Statement& stmt, int index,
                      const std::optional<std::int64_t>& value) {
    if (value.has_value()) {
        stmt.bind(index, *value);
    } else {
        stmt.bind(index);
    }
}

}  // namespace

models::WorkoutSession SessionRepository::create(const models::WorkoutSession& s) {
    // performed_at is left out of the column list when the caller did not supply
    // one, so the table default (datetime('now')) applies.
    if (s.performed_at.empty()) {
        SQLite::Statement stmt(db_,
                               "INSERT INTO workout_sessions (trainee_id, plan_id, status) "
                               "VALUES (?, ?, ?)");
        stmt.bind(1, s.trainee_id);
        bind_optional_id(stmt, 2, s.plan_id);
        stmt.bind(3, s.status);
        stmt.exec();
    } else {
        SQLite::Statement stmt(db_,
                               "INSERT INTO workout_sessions "
                               "(trainee_id, plan_id, performed_at, status) "
                               "VALUES (?, ?, ?, ?)");
        stmt.bind(1, s.trainee_id);
        bind_optional_id(stmt, 2, s.plan_id);
        stmt.bind(3, s.performed_at);
        stmt.bind(4, s.status);
        stmt.exec();
    }
    return find_by_id(db_.getLastInsertRowid()).value();
}

std::optional<models::WorkoutSession> SessionRepository::find_by_id(std::int64_t id) {
    SQLite::Statement stmt(
        db_, std::string("SELECT ") + kSelectColumns + " FROM workout_sessions WHERE id = ?");
    stmt.bind(1, id);
    if (!stmt.executeStep()) {
        return std::nullopt;
    }
    return row_to_session(stmt);
}

std::vector<models::WorkoutSession> SessionRepository::list_by_trainee(std::int64_t trainee_id) {
    SQLite::Statement stmt(db_, std::string("SELECT ") + kSelectColumns +
                                    " FROM workout_sessions WHERE trainee_id = ? "
                                    "ORDER BY performed_at DESC, id DESC");
    stmt.bind(1, trainee_id);

    std::vector<models::WorkoutSession> result;
    while (stmt.executeStep()) {
        result.push_back(row_to_session(stmt));
    }
    return result;
}

bool SessionRepository::update(const models::WorkoutSession& s) {
    SQLite::Statement stmt(db_, "UPDATE workout_sessions SET status = ? WHERE id = ?");
    stmt.bind(1, s.status);
    stmt.bind(2, s.id);
    return stmt.exec() > 0;
}

bool SessionRepository::remove(std::int64_t id) {
    SQLite::Statement stmt(db_, "DELETE FROM workout_sessions WHERE id = ?");
    stmt.bind(1, id);
    return stmt.exec() > 0;
}

}  // namespace fitplan::repositories
