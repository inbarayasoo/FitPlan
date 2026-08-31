#include "repositories/PlanRepository.hpp"

#include <string>

namespace fitplan::repositories {

namespace {

constexpr const char* kSelectColumns =
    "id, coach_id, trainee_id, name, notes, is_active, created_at";

// Column order must match kSelectColumns:
//   0 id  1 coach_id  2 trainee_id  3 name  4 notes  5 is_active  6 created_at
models::WorkoutPlan row_to_plan(SQLite::Statement& stmt) {
    models::WorkoutPlan p;
    p.id = stmt.getColumn(0).getInt64();
    p.coach_id = stmt.getColumn(1).getInt64();
    p.trainee_id = stmt.getColumn(2).getInt64();
    p.name = stmt.getColumn(3).getString();
    if (!stmt.getColumn(4).isNull()) {
        p.notes = stmt.getColumn(4).getString();
    }
    p.is_active = stmt.getColumn(5).getInt() != 0;
    p.created_at = stmt.getColumn(6).getString();
    return p;
}

void bind_optional(SQLite::Statement& stmt, int index,
                   const std::optional<std::string>& value) {
    if (value.has_value()) {
        stmt.bind(index, *value);
    } else {
        stmt.bind(index);
    }
}

}  // namespace

models::WorkoutPlan PlanRepository::create(const models::WorkoutPlan& p) {
    SQLite::Statement stmt(
        db_,
        "INSERT INTO workout_plans (coach_id, trainee_id, name, notes, is_active) "
        "VALUES (?, ?, ?, ?, ?)");
    stmt.bind(1, p.coach_id);
    stmt.bind(2, p.trainee_id);
    stmt.bind(3, p.name);
    bind_optional(stmt, 4, p.notes);
    stmt.bind(5, p.is_active ? 1 : 0);
    stmt.exec();

    return find_by_id(db_.getLastInsertRowid()).value();
}

std::optional<models::WorkoutPlan> PlanRepository::find_by_id(std::int64_t id) {
    SQLite::Statement stmt(
        db_,
        std::string("SELECT ") + kSelectColumns + " FROM workout_plans WHERE id = ?");
    stmt.bind(1, id);
    if (!stmt.executeStep()) {
        return std::nullopt;
    }
    return row_to_plan(stmt);
}

std::vector<models::WorkoutPlan> PlanRepository::list_by_coach(
    std::int64_t coach_id) {
    SQLite::Statement stmt(
        db_, std::string("SELECT ") + kSelectColumns +
                 " FROM workout_plans WHERE coach_id = ? "
                 "ORDER BY created_at DESC, id DESC");
    stmt.bind(1, coach_id);

    std::vector<models::WorkoutPlan> result;
    while (stmt.executeStep()) {
        result.push_back(row_to_plan(stmt));
    }
    return result;
}

std::optional<models::WorkoutPlan> PlanRepository::find_active_for_trainee(
    std::int64_t trainee_id) {
    SQLite::Statement stmt(
        db_, std::string("SELECT ") + kSelectColumns +
                 " FROM workout_plans WHERE trainee_id = ? AND is_active = 1 "
                 "ORDER BY id DESC LIMIT 1");
    stmt.bind(1, trainee_id);
    if (!stmt.executeStep()) {
        return std::nullopt;
    }
    return row_to_plan(stmt);
}

bool PlanRepository::update(const models::WorkoutPlan& p) {
    SQLite::Statement stmt(
        db_, "UPDATE workout_plans SET name = ?, notes = ? WHERE id = ?");
    stmt.bind(1, p.name);
    bind_optional(stmt, 2, p.notes);
    stmt.bind(3, p.id);
    return stmt.exec() > 0;
}

bool PlanRepository::set_active(std::int64_t plan_id, bool active) {
    SQLite::Statement stmt(
        db_, "UPDATE workout_plans SET is_active = ? WHERE id = ?");
    stmt.bind(1, active ? 1 : 0);
    stmt.bind(2, plan_id);
    return stmt.exec() > 0;
}

int PlanRepository::deactivate_all_for_trainee(std::int64_t trainee_id) {
    SQLite::Statement stmt(
        db_,
        "UPDATE workout_plans SET is_active = 0 "
        "WHERE trainee_id = ? AND is_active = 1");
    stmt.bind(1, trainee_id);
    return stmt.exec();
}

bool PlanRepository::remove(std::int64_t id) {
    SQLite::Statement stmt(db_, "DELETE FROM workout_plans WHERE id = ?");
    stmt.bind(1, id);
    return stmt.exec() > 0;
}

}  // namespace fitplan::repositories
