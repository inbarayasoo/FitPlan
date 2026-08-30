#include "repositories/SessionSetRepository.hpp"

#include <optional>
#include <string>

namespace fitplan::repositories {

namespace {

constexpr const char* kSelectColumns =
    "id, session_id, exercise_id, plan_item_id, set_number, reps, weight, rpe, "
    "completed";

// Column order must match kSelectColumns:
//   0 id  1 session_id  2 exercise_id  3 plan_item_id  4 set_number  5 reps
//   6 weight  7 rpe  8 completed
models::SessionSet row_to_set(SQLite::Statement& stmt) {
    models::SessionSet s;
    s.id = stmt.getColumn(0).getInt64();
    s.session_id = stmt.getColumn(1).getInt64();
    s.exercise_id = stmt.getColumn(2).getInt64();
    if (!stmt.getColumn(3).isNull())
        s.plan_item_id = stmt.getColumn(3).getInt64();
    s.set_number = stmt.getColumn(4).getInt();
    if (!stmt.getColumn(5).isNull()) s.reps = stmt.getColumn(5).getInt();
    if (!stmt.getColumn(6).isNull()) s.weight = stmt.getColumn(6).getDouble();
    if (!stmt.getColumn(7).isNull()) s.rpe = stmt.getColumn(7).getDouble();
    s.completed = stmt.getColumn(8).getInt() != 0;
    return s;
}

template <class T>
void bind_optional(SQLite::Statement& stmt, int index, const std::optional<T>& v) {
    if (v.has_value()) {
        stmt.bind(index, *v);
    } else {
        stmt.bind(index);
    }
}

}  // namespace

models::SessionSet SessionSetRepository::create(const models::SessionSet& set) {
    SQLite::Statement stmt(
        db_,
        "INSERT INTO session_sets "
        "(session_id, exercise_id, plan_item_id, set_number, reps, weight, rpe, "
        " completed) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    stmt.bind(1, set.session_id);
    stmt.bind(2, set.exercise_id);
    bind_optional(stmt, 3, set.plan_item_id);
    stmt.bind(4, set.set_number);
    bind_optional(stmt, 5, set.reps);
    bind_optional(stmt, 6, set.weight);
    bind_optional(stmt, 7, set.rpe);
    stmt.bind(8, set.completed ? 1 : 0);
    stmt.exec();

    const std::int64_t new_id = db_.getLastInsertRowid();
    SQLite::Statement back(db_, std::string("SELECT ") + kSelectColumns +
                                   " FROM session_sets WHERE id = ?");
    back.bind(1, new_id);
    back.executeStep();
    return row_to_set(back);
}

std::vector<models::SessionSet> SessionSetRepository::list_by_session(
    std::int64_t session_id) {
    SQLite::Statement stmt(db_, std::string("SELECT ") + kSelectColumns +
                                    " FROM session_sets WHERE session_id = ? "
                                    "ORDER BY set_number ASC, id ASC");
    stmt.bind(1, session_id);

    std::vector<models::SessionSet> result;
    while (stmt.executeStep()) {
        result.push_back(row_to_set(stmt));
    }
    return result;
}

int SessionSetRepository::delete_by_session(std::int64_t session_id) {
    SQLite::Statement stmt(db_, "DELETE FROM session_sets WHERE session_id = ?");
    stmt.bind(1, session_id);
    return stmt.exec();
}

}  // namespace fitplan::repositories
