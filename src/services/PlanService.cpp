#include "services/PlanService.hpp"

#include "services/PlanError.hpp"
#include <string>

#include "models/Exercise.hpp"

namespace fitplan::services {


models::WorkoutPlan PlanService::owned_plan_or_throw(std::int64_t coach_id,
                                                    std::int64_t plan_id) {
    const std::optional<models::WorkoutPlan> found = plans_.find_by_id(plan_id);
    if (!found || found->coach_id != coach_id) {
        throw PlanError(PlanErrorKind::kNotFound, "no plan with that id");
    }
    return *found;
}

void PlanService::validate_and_check_ownership(std::int64_t coach_id,
                                              const PlanInput& input) {
    if (input.name.find_first_not_of(" \t\r\n") == std::string::npos) {
        throw PlanError(PlanErrorKind::kInvalidInput,
                        "plan name must not be blank");
    }
    if (input.items.empty()) {
        throw PlanError(PlanErrorKind::kInvalidInput,
                        "a plan needs at least one item");
    }
    if (!roster_.is_linked(coach_id, input.trainee_id)) {
        throw PlanError(PlanErrorKind::kForbidden,
                        "that trainee is not on your roster");
    }

    for (const PlanItemInput& item : input.items) {
        const std::optional<models::Exercise> ex =
            exercises_.find_by_id(item.exercise_id);
        if (!ex || ex->coach_id != coach_id) {
            throw PlanError(PlanErrorKind::kForbidden,
                            "an item uses an exercise that is not in your library");
        }
        if (item.target_sets && *item.target_sets <= 0) {
            throw PlanError(PlanErrorKind::kInvalidInput,
                            "target_sets must be positive");
        }
        if (item.target_reps && *item.target_reps <= 0) {
            throw PlanError(PlanErrorKind::kInvalidInput,
                            "target_reps must be positive");
        }
        if (item.rest_seconds && *item.rest_seconds < 0) {
            throw PlanError(PlanErrorKind::kInvalidInput,
                            "rest_seconds must not be negative");
        }
        if (item.target_weight && *item.target_weight < 0.0) {
            throw PlanError(PlanErrorKind::kInvalidInput,
                            "target_weight must not be negative");
        }
    }
}

std::vector<models::PlanItem> PlanService::replace_items(
    std::int64_t plan_id, const std::vector<PlanItemInput>& inputs) {
    items_.delete_by_plan(plan_id);

    std::vector<models::PlanItem> stored;
    stored.reserve(inputs.size());

    for (std::size_t i = 0; i < inputs.size(); ++i) {
        const PlanItemInput& in = inputs[i];

        models::PlanItem row;
        row.plan_id = plan_id;
        row.exercise_id = in.exercise_id;
        row.order_index = static_cast<int>(i);
        row.day_label = in.day_label;
        row.target_sets = in.target_sets;
        row.target_reps = in.target_reps;
        row.target_weight = in.target_weight;
        row.rest_seconds = in.rest_seconds;
        row.notes = in.notes;
        row.video_url = in.video_url;

        stored.push_back(items_.create(row));
    }
    return stored;
}

PlanWithItems PlanService::create_plan(std::int64_t coach_id,
                                       const PlanInput& input) {
    validate_and_check_ownership(coach_id, input);

    SQLite::Transaction tx(db_);

    models::WorkoutPlan header;
    header.coach_id = coach_id;
    header.trainee_id = input.trainee_id;
    header.name = input.name;
    header.notes = input.notes;
    header.is_active = false;
    const models::WorkoutPlan saved = plans_.create(header);

    std::vector<models::PlanItem> stored = replace_items(saved.id, input.items);

    tx.commit();

    return PlanWithItems{plans_.find_by_id(saved.id).value(), std::move(stored)};
}

PlanWithItems PlanService::assign_plan(std::int64_t coach_id,
                                       std::int64_t plan_id) {
    const models::WorkoutPlan plan = owned_plan_or_throw(coach_id, plan_id);

    SQLite::Transaction tx(db_);
    plans_.deactivate_all_for_trainee(plan.trainee_id);
    plans_.set_active(plan_id, true);
    tx.commit();

    return PlanWithItems{plans_.find_by_id(plan_id).value(),
                         items_.list_by_plan(plan_id)};
}


PlanWithItems PlanService::update_plan(std::int64_t coach_id,
                                       std::int64_t plan_id,
                                       const PlanInput& input) {
    owned_plan_or_throw(coach_id, plan_id);
    validate_and_check_ownership(coach_id, input);

    SQLite::Transaction tx(db_);

    models::WorkoutPlan header = plans_.find_by_id(plan_id).value();
    header.name = input.name;
    header.notes = input.notes;
    plans_.update(header);

    std::vector<models::PlanItem> stored = replace_items(plan_id, input.items);

    tx.commit();

    return PlanWithItems{plans_.find_by_id(plan_id).value(), std::move(stored)};
}

std::vector<models::WorkoutPlan> PlanService::list_plans(std::int64_t coach_id) {
    return plans_.list_by_coach(coach_id);
}

PlanWithItems PlanService::get_plan(std::int64_t coach_id,
                                    std::int64_t plan_id) {
    const models::WorkoutPlan header = owned_plan_or_throw(coach_id, plan_id);
    return PlanWithItems{header, items_.list_by_plan(plan_id)};
}

}  // namespace fitplan::services
