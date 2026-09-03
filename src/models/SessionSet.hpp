#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace fitplan::models {

// Mirrors one row of the `session_sets` table: a single set the trainee logged.
// plan_item_id links back to what was prescribed, or std::nullopt for ad-hoc.
struct SessionSet {
    std::int64_t id = 0;
    std::int64_t session_id = 0;
    std::int64_t exercise_id = 0;
    // The referenced exercise's name. Filled by the repository on every read
    // (a JOIN to `exercises`); ignored on write.
    std::string exercise_name;
    std::optional<std::int64_t> plan_item_id;
    int set_number = 0;
    std::optional<int> reps;
    std::optional<double> weight;
    std::optional<double> rpe;
    bool completed = true;
    // The trainee's free-text note for this one set ("felt strong", "left
    // shoulder tight"). std::nullopt when they left it blank.
    std::optional<std::string> notes;
};

}  // namespace fitplan::models
