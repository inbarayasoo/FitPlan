#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "models/WorkoutPlan.hpp"

namespace fitplan::repositories {

// Data access for the `workout_plans` table. Same shape as the other
// repositories: all SQL for plans lives here, and it borrows the connection.
// The nested plan_items are a separate repository (PlanItemRepository); a
// PlanService stitches the two together.
class PlanRepository {
public:
    explicit PlanRepository(SQLite::Database& db) : db_(db) {}

    // Inserts a plan header. Uses p.coach_id, p.trainee_id, p.name, p.notes,
    // p.is_active. Returns the stored plan with id and created_at filled in.
    models::WorkoutPlan create(const models::WorkoutPlan& p);

    std::optional<models::WorkoutPlan> find_by_id(std::int64_t id);

    // Every plan this coach owns, newest first.
    std::vector<models::WorkoutPlan> list_by_coach(std::int64_t coach_id);

    // Updates the editable header fields (name, notes) of the row with p.id.
    // Activation is a separate concern - see set_active(). Returns true if a row
    // changed.
    bool update(const models::WorkoutPlan& p);

    // Flips one plan's is_active flag. Returns true if a row changed.
    bool set_active(std::int64_t plan_id, bool active);

    // Clears is_active on every plan belonging to this trainee. Returns the
    // number of rows changed. Used right before activating a new plan so a
    // trainee never has two active plans at once.
    int deactivate_all_for_trainee(std::int64_t trainee_id);

private:
    SQLite::Database& db_;
};

}  // namespace fitplan::repositories
