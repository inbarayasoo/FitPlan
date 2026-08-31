#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace fitplan::models {

// Mirrors one row of the `plan_items` table: a single exercise slot within a
// plan, with its prescribed targets. All targets are optional so a coach can
// leave any of them blank.
struct PlanItem {
    std::int64_t id = 0;
    std::int64_t plan_id = 0;
    std::int64_t exercise_id = 0;
    // The referenced exercise's name. Filled by the repository on every read
    // (a JOIN to `exercises`); ignored on write - `exercise_id` is the FK.
    std::string exercise_name;
    int order_index = 0;
    std::optional<std::string> day_label;
    std::optional<int> target_sets;
    std::optional<int> target_reps;
    std::optional<double> target_weight;
    std::optional<int> rest_seconds;
    std::optional<std::string> notes;
    std::optional<std::string> video_url;
};

}  // namespace fitplan::models
