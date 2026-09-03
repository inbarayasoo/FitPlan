#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace fitplan::models {

// Mirrors one row of the `workout_sessions` table: a workout a trainee actually
// performed. plan_id is optional (ad-hoc sessions have no plan). Per-set notes
// live on session_sets; there is no session-level note.
struct WorkoutSession {
    std::int64_t id = 0;
    std::int64_t trainee_id = 0;
    std::optional<std::int64_t> plan_id;
    std::string performed_at;
    std::string status;  // "planned" | "in_progress" | "completed"
};

}  // namespace fitplan::models
