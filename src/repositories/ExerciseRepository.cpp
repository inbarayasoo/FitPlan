#include "repositories/ExerciseRepository.hpp"

#include <string>

namespace fitplan::repositories {

namespace {

// The column list every SELECT below uses, in this exact order.
constexpr const char* kSelectColumns =
    "id, coach_id, name, category, primary_muscle, description, video_url, created_at";

// Reads the current row of `stmt` into an Exercise. The column order here must
// match kSelectColumns:
//   0 id   1 coach_id   2 name   3 category   4 primary_muscle
//   5 description   6 video_url   7 created_at
models::Exercise row_to_exercise(SQLite::Statement& stmt) {
    models::Exercise e;
    e.id = stmt.getColumn(0).getInt64();
    e.coach_id = stmt.getColumn(1).getInt64();
    e.name = stmt.getColumn(2).getString();
    if (!stmt.getColumn(3).isNull()) {
        e.category = stmt.getColumn(3).getString();
    }
    if (!stmt.getColumn(4).isNull()) {
        e.primary_muscle = stmt.getColumn(4).getString();
    }
    if (!stmt.getColumn(5).isNull()) {
        e.description = stmt.getColumn(5).getString();
    }
    if (!stmt.getColumn(6).isNull()) {
        e.video_url = stmt.getColumn(6).getString();
    }
    e.created_at = stmt.getColumn(7).getString();
    return e;
}

// Binds an optional string to parameter `index`: the string value if present,
// otherwise SQL NULL.
void bind_optional(SQLite::Statement& stmt, int index,
                   const std::optional<std::string>& value) {
    if (value.has_value()) {
        stmt.bind(index, *value);
    } else {
        stmt.bind(index);  // one argument => bind SQL NULL
    }
}

}  // namespace

models::Exercise ExerciseRepository::create(const models::Exercise& e) {
    SQLite::Statement stmt(
        db_,
        "INSERT INTO exercises "
        "(coach_id, name, category, primary_muscle, description, video_url) "
        "VALUES (?, ?, ?, ?, ?, ?)");

    stmt.bind(1, e.coach_id);
    stmt.bind(2, e.name);
    bind_optional(stmt, 3, e.category);
    bind_optional(stmt, 4, e.primary_muscle);
    bind_optional(stmt, 5, e.description);
    bind_optional(stmt, 6, e.video_url);

    stmt.exec();

    return find_by_id(db_.getLastInsertRowid()).value();
}

std::optional<models::Exercise> ExerciseRepository::find_by_id(std::int64_t id) {
    SQLite::Statement stmt(
        db_, std::string("SELECT ") + kSelectColumns + " FROM exercises WHERE id = ?");
    stmt.bind(1, id);

    if (!stmt.executeStep()) {
        return std::nullopt;
    }
    return row_to_exercise(stmt);
}

std::vector<models::Exercise> ExerciseRepository::list_by_coach(std::int64_t coach_id) {
    SQLite::Statement stmt(
        db_, std::string("SELECT ") + kSelectColumns +
                 " FROM exercises WHERE coach_id = ? ORDER BY created_at DESC, id DESC");
    stmt.bind(1, coach_id);

    std::vector<models::Exercise> result;
    while (stmt.executeStep()) {
        result.push_back(row_to_exercise(stmt));
    }
    return result;
}

bool ExerciseRepository::update(const models::Exercise& e) {
    SQLite::Statement stmt(
        db_,
        "UPDATE exercises SET name = ?, category = ?, primary_muscle = ?, "
        "description = ?, video_url = ? WHERE id = ?");
    stmt.bind(1, e.name);
    bind_optional(stmt, 2, e.category);
    bind_optional(stmt, 3, e.primary_muscle);
    bind_optional(stmt, 4, e.description);
    bind_optional(stmt, 5, e.video_url);
    stmt.bind(6, e.id);
    return stmt.exec() > 0;  // rows changed
}

bool ExerciseRepository::remove(std::int64_t id) {
    SQLite::Statement stmt(db_, "DELETE FROM exercises WHERE id = ?");
    stmt.bind(1, id);
    return stmt.exec() > 0;  // rows deleted
}

}  // namespace fitplan::repositories
