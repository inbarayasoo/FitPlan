#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "models/PlanItem.hpp"
#include "models/WorkoutPlan.hpp"
#include "repositories/CoachTraineeRepository.hpp"
#include "repositories/ExerciseRepository.hpp"
#include "repositories/PlanItemRepository.hpp"
#include "repositories/PlanRepository.hpp"

namespace fitplan::services {

// One exercise slot as it arrives from the caller. Note what is NOT here:
// order_index (the service assigns it from list position) and plan_id (known
// only after the header row is inserted).
struct PlanItemInput {
    std::int64_t exercise_id = 0;
    std::optional<std::string> day_label;
    std::optional<int> target_sets;
    std::optional<int> target_reps;
    std::optional<double> target_weight;
    std::optional<int> rest_seconds;
    std::optional<std::string> notes;
    std::optional<std::string> video_url;  // per-item override of the library link
};

// A whole plan as the caller wants it: a header plus its ordered item list.
struct PlanInput {
    std::int64_t trainee_id = 0;
    std::string name;
    std::optional<std::string> notes;
    std::vector<PlanItemInput> items;
};

// What every read/write returns: the stored header and its stored items.
struct PlanWithItems {
    models::WorkoutPlan plan;
    std::vector<models::PlanItem> items;
};

// Business logic for workout plans. Crow-free. Coordinates four repositories and
// owns the transaction boundary for the multi-row writes (header + N items must
// land together or not at all).
//
// Every method takes the caller's coach_id and scopes to it: a coach can only
// see or change their own plans, can only build a plan for a trainee on their
// roster, and can only reference exercises from their own library.
class PlanService {
public:
    PlanService(SQLite::Database& db, repositories::PlanRepository& plans,
                repositories::PlanItemRepository& items,
                repositories::CoachTraineeRepository& roster,
                repositories::ExerciseRepository& exercises)
        : db_(db), plans_(plans), items_(items), roster_(roster), exercises_(exercises) {}

    // Insert a new plan (header + items). Throws PlanError:
    //   kInvalidInput - blank name, empty item list, bad target value
    //   kForbidden    - trainee not on the roster, or an item's exercise is not
    //                   this coach's
    PlanWithItems create_plan(std::int64_t coach_id, const PlanInput& input);

    // Replace a plan's editable header fields and its entire item list.
    // Throws PlanError kNotFound / kInvalidInput / kForbidden as above.
    PlanWithItems update_plan(std::int64_t coach_id, std::int64_t plan_id, const PlanInput& input);

    // All of this coach's plans, headers only, newest first.
    std::vector<models::WorkoutPlan> list_plans(std::int64_t coach_id);

    // One plan with its items. Throws PlanError(kNotFound) if it is not this
    // coach's.
    PlanWithItems get_plan(std::int64_t coach_id, std::int64_t plan_id);

    // Make this plan the trainee's active one, deactivating any other active
    // plan they have. Throws PlanError(kNotFound) if it is not this coach's.
    PlanWithItems assign_plan(std::int64_t coach_id, std::int64_t plan_id);

    // Delete one of this coach's plans (its items cascade). Throws
    // PlanError(kNotFound) if it is missing or owned by a different coach.
    void delete_plan(std::int64_t coach_id, std::int64_t plan_id);

private:
    // Load the plan or throw PlanError(kNotFound) when it is missing or owned by
    // a different coach.
    models::WorkoutPlan owned_plan_or_throw(std::int64_t coach_id, std::int64_t plan_id);

    // Validate `input` and confirm the caller may reference everything in it:
    // non-blank name, at least one item, positive targets, trainee on the
    // roster, every item's exercise in the caller's library.
    void validate_and_check_ownership(std::int64_t coach_id, const PlanInput& input);

    // Delete every existing item of `plan_id`, then insert `inputs` in order,
    // giving each a zero-based order_index from its position. Returns the stored
    // items. Assumes it runs inside a transaction opened by the caller.
    std::vector<models::PlanItem> replace_items(std::int64_t plan_id,
                                                const std::vector<PlanItemInput>& inputs);

    SQLite::Database& db_;
    repositories::PlanRepository& plans_;
    repositories::PlanItemRepository& items_;
    repositories::CoachTraineeRepository& roster_;
    repositories::ExerciseRepository& exercises_;
};

}  // namespace fitplan::services
