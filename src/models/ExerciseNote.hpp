#pragma once

#include <cstdint>
#include <string>

namespace fitplan::models {

// Mirrors one row of the `exercise_notes` table: a trainee's free-text note for
// one exercise. `exercise_name` is filled by the repository on reads (a JOIN to
// `exercises`) and ignored on writes.
struct ExerciseNote {
    std::int64_t id = 0;
    std::int64_t trainee_id = 0;
    std::int64_t exercise_id = 0;
    std::string exercise_name;
    std::string body;
    std::string updated_at;
};

}  // namespace fitplan::models
