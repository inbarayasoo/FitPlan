#include "repositories/ExerciseNoteRepository.hpp"

#include <string>

namespace fitplan::repositories {

namespace {

constexpr const char* kSelectColumns =
    "en.id, en.trainee_id, en.exercise_id, e.name, en.body, en.updated_at";

constexpr const char* kFromJoin =
    " FROM exercise_notes en JOIN exercises e ON e.id = en.exercise_id";

// Column order must match kSelectColumns:
//   0 id  1 trainee_id  2 exercise_id  3 exercise name  4 body  5 updated_at
models::ExerciseNote row_to_note(SQLite::Statement& stmt) {
    models::ExerciseNote n;
    n.id = stmt.getColumn(0).getInt64();
    n.trainee_id = stmt.getColumn(1).getInt64();
    n.exercise_id = stmt.getColumn(2).getInt64();
    n.exercise_name = stmt.getColumn(3).getString();
    n.body = stmt.getColumn(4).getString();
    n.updated_at = stmt.getColumn(5).getString();
    return n;
}

}  // namespace

std::vector<models::ExerciseNote> ExerciseNoteRepository::list_for_trainee(
    std::int64_t trainee_id) {
    SQLite::Statement stmt(db_, std::string("SELECT ") + kSelectColumns +
                                   kFromJoin +
                                   " WHERE en.trainee_id = ? "
                                   "ORDER BY en.updated_at DESC, en.id DESC");
    stmt.bind(1, trainee_id);

    std::vector<models::ExerciseNote> result;
    while (stmt.executeStep()) {
        result.push_back(row_to_note(stmt));
    }
    return result;
}

std::optional<models::ExerciseNote> ExerciseNoteRepository::find(
    std::int64_t trainee_id, std::int64_t exercise_id) {
    SQLite::Statement stmt(
        db_, std::string("SELECT ") + kSelectColumns + kFromJoin +
                 " WHERE en.trainee_id = ? AND en.exercise_id = ?");
    stmt.bind(1, trainee_id);
    stmt.bind(2, exercise_id);
    if (!stmt.executeStep()) {
        return std::nullopt;
    }
    return row_to_note(stmt);
}

models::ExerciseNote ExerciseNoteRepository::upsert(std::int64_t trainee_id,
                                                    std::int64_t exercise_id,
                                                    const std::string& body) {
    SQLite::Statement stmt(
        db_,
        "INSERT INTO exercise_notes (trainee_id, exercise_id, body) "
        "VALUES (?, ?, ?) "
        "ON CONFLICT (trainee_id, exercise_id) DO UPDATE SET "
        "  body = excluded.body, updated_at = datetime('now')");
    stmt.bind(1, trainee_id);
    stmt.bind(2, exercise_id);
    stmt.bind(3, body);
    stmt.exec();
    return find(trainee_id, exercise_id).value();
}

bool ExerciseNoteRepository::remove(std::int64_t trainee_id,
                                    std::int64_t exercise_id) {
    SQLite::Statement stmt(
        db_,
        "DELETE FROM exercise_notes WHERE trainee_id = ? AND exercise_id = ?");
    stmt.bind(1, trainee_id);
    stmt.bind(2, exercise_id);
    return stmt.exec() > 0;
}

}  // namespace fitplan::repositories
