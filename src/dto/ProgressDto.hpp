#pragma once

#include <crow.h>

#include <cstdint>
#include <string>
#include <vector>

#include "services/ProgressService.hpp"

namespace fitplan::dto {

// Best-e1RM time series for a single exercise.
struct ExerciseE1rmSeries {
    std::int64_t exercise_id = 0;
    std::string exercise_name;  // looked up by the controller for display
    std::vector<services::SeriesPoint> series;
};

// Everything GET /api/my/progress (and the coach's roster-scoped view) returns,
// already computed by ProgressService. The DTO only turns it into JSON.
struct ProgressReport {
    std::int64_t trainee_id = 0;
    std::string as_of;  // "YYYY-MM-DD" the streak was measured against
    double total_volume = 0.0;
    double adherence = 0.0;  // 0..1
    int weekly_streak = 0;
    std::vector<services::SeriesPoint> volume_over_time;
    std::vector<ExerciseE1rmSeries> exercises;
};

crow::response progress_response(const ProgressReport& report);

}  // namespace fitplan::dto
