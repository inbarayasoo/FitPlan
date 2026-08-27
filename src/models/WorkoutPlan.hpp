#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace fitplan::models {

// Mirrors one row of the `workout_plans` table.
struct WorkoutPlan {
    std::int64_t id = 0;
    std::int64_t coach_id = 0;
    std::int64_t trainee_id = 0;
    std::string name;
    std::optional<std::string> notes;
    bool is_active = false;
    std::string created_at;
};

}  // namespace fitplan::models
