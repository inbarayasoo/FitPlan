#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <map>
#include <chrono>

namespace fitplan::services {

// ---------------------------------------------------------------------------
// Flattened inputs. The controller builds these from repository rows and hands
// them to ProgressService, which never touches the database itself. That is
// what makes every function below pure: same input -> same output, no I/O.
// ---------------------------------------------------------------------------

// One set a trainee logged, with just the fields the progress maths needs.
// `performed_on` is the calendar day of the parent session, "YYYY-MM-DD".
struct LoggedSet {
    std::int64_t exercise_id = 0;
    std::string performed_on;
    std::optional<int> reps;
    std::optional<double> weight;
    bool completed = true;
    std::optional<std::int64_t> plan_item_id;  // set it was prescribed against
};

// One prescribed exercise slot from the trainee's active plan: how many sets
// were asked for. Used only by adherence().
struct PrescribedItem {
    std::int64_t plan_item_id = 0;
    int target_sets = 0;
};

// A dated point in a time series, sorted ascending by `date`, one per calendar
// day that has data.
struct SeriesPoint {
    std::string date;  // "YYYY-MM-DD"
    double value = 0.0;
};

// Pure progress calculations over a trainee's logged sets. Every method is
// static: the class is a namespace with the name the architecture doc uses.
class ProgressService {
public:
    // Epley estimated one-rep max for a single set: weight * (1 + reps / 30).
    // std::nullopt when reps or weight is missing, or either is <= 0.
    static std::optional<double> epley_1rm(const LoggedSet& set);

    // Total training volume = sum(reps * weight) over every completed set that
    // has both values and both > 0. Any other set contributes nothing.
    static double total_volume(const std::vector<LoggedSet>& sets);

    // total_volume grouped by calendar day, ascending by date. One point per
    // day that has at least one qualifying set.
    static std::vector<SeriesPoint> volume_over_time(
        const std::vector<LoggedSet>& sets);

    // Highest epley_1rm per calendar day for one exercise, ascending by date.
    // Filters to `exercise_id` first; days with no valid e1RM are skipped.
    static std::vector<SeriesPoint> best_e1rm_over_time(
        const std::vector<LoggedSet>& sets, std::int64_t exercise_id);

    // Adherence in [0, 1] = completed sets that name a prescribed plan item,
    // divided by the total prescribed set count. 0.0 when nothing is
    // prescribed; capped at 1.0 if more sets were logged than asked for.
    static double adherence(const std::vector<LoggedSet>& sets,
                            const std::vector<PrescribedItem>& prescribed);

    // Consecutive ISO-8601 weeks ending with the week of `as_of` (a
    // "YYYY-MM-DD" date, normally today) in which the trainee logged at least
    // one set. 0 if they did not train in the week of `as_of`.
    static int weekly_streak(const std::vector<LoggedSet>& sets,
                             const std::string& as_of);

private:
    // A single set's contribution to training volume: reps * weight when the
    // set was completed and both values are positive, otherwise 0.
    static double set_volume(const LoggedSet& set);
        // Collapse a date -> value map into a series sorted ascending by date.
    static std::vector<SeriesPoint> to_series(
        const std::map<std::string, double>& by_day);
        // Parse a "YYYY-MM-DD" date. std::nullopt if it is not a valid date.
    static std::optional<std::chrono::sys_days> parse_iso_date(
        const std::string& text);

    // The Monday that starts the week containing `day`. Two dates share a week
    // exactly when this returns the same value for both.
    static std::chrono::sys_days monday_of_week(std::chrono::sys_days day);
};

}  // namespace fitplan::services