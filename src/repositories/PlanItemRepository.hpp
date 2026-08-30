#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "models/PlanItem.hpp"

namespace fitplan::repositories {

// Data access for the `plan_items` table - the exercise slots inside a plan.
// There is no stand-alone update: a PlanService edits a plan's items by wiping
// them (delete_by_plan) and re-inserting the new list, inside one transaction.
class PlanItemRepository {
public:
    explicit PlanItemRepository(SQLite::Database& db) : db_(db) {}

    // Inserts one item. Uses every field of `item` except id. Returns it with
    // the database-assigned id filled in.
    models::PlanItem create(const models::PlanItem& item);

    std::optional<models::PlanItem> find_by_id(std::int64_t id);

    // All items for a plan, in presentation order (order_index, then id).
    std::vector<models::PlanItem> list_by_plan(std::int64_t plan_id);

    // Removes every item of a plan. Returns the number of rows deleted.
    int delete_by_plan(std::int64_t plan_id);

private:
    SQLite::Database& db_;
};

}  // namespace fitplan::repositories
