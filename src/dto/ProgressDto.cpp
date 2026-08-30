#include "dto/ProgressDto.hpp"

#include <nlohmann/json.hpp>

#include "http/Json.hpp"

namespace fitplan::dto {

namespace {

using nlohmann::json;

json series_to_json(const std::vector<services::SeriesPoint>& series) {
    json arr = json::array();
    for (const services::SeriesPoint& p : series) {
        arr.push_back(json{{"date", p.date}, {"value", p.value}});
    }
    return arr;
}

}  // namespace

crow::response progress_response(const ProgressReport& report) {
    json exercises = json::array();
    for (const ExerciseE1rmSeries& ex : report.exercises) {
        exercises.push_back(json{
            {"exercise_id", ex.exercise_id},
            {"best_e1rm_over_time", series_to_json(ex.series)},
        });
    }

    const json body{
        {"trainee_id", report.trainee_id},
        {"as_of", report.as_of},
        {"total_volume", report.total_volume},
        {"adherence", report.adherence},
        {"weekly_streak", report.weekly_streak},
        {"volume_over_time", series_to_json(report.volume_over_time)},
        {"exercises", std::move(exercises)},
    };
    return http::json_response(200, body);
}

}  // namespace fitplan::dto
