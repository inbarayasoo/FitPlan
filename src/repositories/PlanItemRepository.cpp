#include "repositories/PlanItemRepository.hpp"

#include <string>

namespace fitplan::repositories {

namespace {

// Qualified with the `pi` alias because every read JOINs `exercises e` to pick
// up the exercise name, and both tables have an `id` column.
constexpr const char* kSelectColumns =
    "pi.id, pi.plan_id, pi.exercise_id, pi.order_index, pi.day_label, "
    "pi.target_sets, pi.target_reps, pi.target_weight, pi.rest_seconds, "
    "pi.notes, pi.video_url, e.name";

constexpr const char* kFromJoin =
    " FROM plan_items pi JOIN exercises e ON e.id = pi.exercise_id";

// Column order must match kSelectColumns:
//   0 id  1 plan_id  2 exercise_id  3 order_index  4 day_label  5 target_sets
//   6 target_reps  7 target_weight  8 rest_seconds  9 notes  10 video_url
//   11 exercise name
models::PlanItem row_to_item(SQLite::Statement& stmt) {
    models::PlanItem it;
    it.id = stmt.getColumn(0).getInt64();
    it.plan_id = stmt.getColumn(1).getInt64();
    it.exercise_id = stmt.getColumn(2).getInt64();
    it.order_index = stmt.getColumn(3).getInt();
    if (!stmt.getColumn(4).isNull()) it.day_label = stmt.getColumn(4).getString();
    if (!stmt.getColumn(5).isNull()) it.target_sets = stmt.getColumn(5).getInt();
    if (!stmt.getColumn(6).isNull()) it.target_reps = stmt.getColumn(6).getInt();
    if (!stmt.getColumn(7).isNull()) it.target_weight = stmt.getColumn(7).getDouble();
    if (!stmt.getColumn(8).isNull()) it.rest_seconds = stmt.getColumn(8).getInt();
    if (!stmt.getColumn(9).isNull()) it.notes = stmt.getColumn(9).getString();
    if (!stmt.getColumn(10).isNull()) it.video_url = stmt.getColumn(10).getString();
    it.exercise_name = stmt.getColumn(11).getString();
    return it;
}

// Bind an optional of any SQLiteCpp-bindable type: the value if present, else
// SQL NULL. One template covers std::string, int, and double.
template <class T>
void bind_optional(SQLite::Statement& stmt, int index, const std::optional<T>& v) {
    if (v.has_value()) {
        stmt.bind(index, *v);
    } else {
        stmt.bind(index);
    }
}

}  // namespace

models::PlanItem PlanItemRepository::create(const models::PlanItem& item) {
    SQLite::Statement stmt(
        db_,
        "INSERT INTO plan_items "
        "(plan_id, exercise_id, order_index, day_label, target_sets, "
        " target_reps, target_weight, rest_seconds, notes, video_url) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    stmt.bind(1, item.plan_id);
    stmt.bind(2, item.exercise_id);
    stmt.bind(3, item.order_index);
    bind_optional(stmt, 4, item.day_label);
    bind_optional(stmt, 5, item.target_sets);
    bind_optional(stmt, 6, item.target_reps);
    bind_optional(stmt, 7, item.target_weight);
    bind_optional(stmt, 8, item.rest_seconds);
    bind_optional(stmt, 9, item.notes);
    bind_optional(stmt, 10, item.video_url);
    stmt.exec();

    return find_by_id(db_.getLastInsertRowid()).value();
}

std::optional<models::PlanItem> PlanItemRepository::find_by_id(std::int64_t id) {
    SQLite::Statement stmt(
        db_,
        std::string("SELECT ") + kSelectColumns + kFromJoin + " WHERE pi.id = ?");
    stmt.bind(1, id);
    if (!stmt.executeStep()) {
        return std::nullopt;
    }
    return row_to_item(stmt);
}

std::vector<models::PlanItem> PlanItemRepository::list_by_plan(
    std::int64_t plan_id) {
    SQLite::Statement stmt(
        db_, std::string("SELECT ") + kSelectColumns + kFromJoin +
                 " WHERE pi.plan_id = ? "
                 "ORDER BY pi.order_index ASC, pi.id ASC");
    stmt.bind(1, plan_id);

    std::vector<models::PlanItem> result;
    while (stmt.executeStep()) {
        result.push_back(row_to_item(stmt));
    }
    return result;
}

int PlanItemRepository::delete_by_plan(std::int64_t plan_id) {
    SQLite::Statement stmt(db_, "DELETE FROM plan_items WHERE plan_id = ?");
    stmt.bind(1, plan_id);
    return stmt.exec();
}

}  // namespace fitplan::repositories
