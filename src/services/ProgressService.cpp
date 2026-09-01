#include "services/ProgressService.hpp"
#include <algorithm>
#include <chrono>
#include <map>
#include <set>
#include <sstream>

namespace fitplan::services {

double ProgressService::set_volume(const LoggedSet& set) {
    if (!set.completed) {
        return 0.0;
    }
    if (!set.reps || !set.weight) {
        return 0.0;
    }
    if (*set.reps <= 0 || *set.weight <= 0.0) {
        return 0.0;
    }
    return *set.reps * *set.weight;
}

double ProgressService::total_volume(const std::vector<LoggedSet>& sets) {
    double total = 0.0;
    for (const LoggedSet& set : sets) {
        total += set_volume(set);
    }
    return total;
}

std::vector<SeriesPoint> ProgressService::to_series(const std::map<std::string, double>& by_day) {
    std::vector<SeriesPoint> series;
    series.reserve(by_day.size());
    for (const auto& [date, value] : by_day) {
        series.push_back({date, value});
    }
    return series;
}

std::vector<SeriesPoint> ProgressService::volume_over_time(const std::vector<LoggedSet>& sets) {
    std::map<std::string, double> by_day;
    for (const LoggedSet& set : sets) {
        const double v = set_volume(set);
        if (v > 0.0) {
            by_day[set.performed_on] += v;
        }
    }
    return to_series(by_day);
}

std::vector<SeriesPoint> ProgressService::best_e1rm_over_time(const std::vector<LoggedSet>& sets,
                                                              std::int64_t exercise_id) {
    std::map<std::string, double> best_by_day;
    for (const LoggedSet& set : sets) {
        // 1. only this exercise
        if (set.exercise_id != exercise_id) {
            continue;
        }
        // 2. only sets that were actually completed
        if (!set.completed) {
            continue;
        }
        // 3. skip sets with no valid estimate
        const std::optional<double> e1rm = epley_1rm(set);
        if (!e1rm) {
            continue;
        }
        // 4. keep the highest estimate seen for that day
        double& best = best_by_day[set.performed_on];
        best = std::max(best, *e1rm);
    }
    return to_series(best_by_day);
}

double ProgressService::adherence(const std::vector<LoggedSet>& sets,
                                  const std::vector<PrescribedItem>& prescribed) {
    int total_prescribed = 0;
    std::set<std::int64_t> prescribed_ids;
    for (const PrescribedItem& item : prescribed) {
        if (item.target_sets > 0) {
            total_prescribed += item.target_sets;
            prescribed_ids.insert(item.plan_item_id);
        }
    }
    if (total_prescribed == 0) {
        return 0.0;
    }

    int completed_prescribed = 0;
    for (const LoggedSet& set : sets) {
        // count a set only if it was completed, names a plan item, and that
        // item is one we actually prescribed
        if (!set.completed) {
            continue;
        }
        if (!set.plan_item_id) {
            continue;
        }
        if (!prescribed_ids.contains(*set.plan_item_id)) {
            continue;
        }
        ++completed_prescribed;
    }

    const double ratio = static_cast<double>(completed_prescribed) / total_prescribed;
    return std::min(ratio, 1.0);
}

std::optional<double> ProgressService::epley_1rm(const LoggedSet& set) {
    // 1. no estimate is possible without both numbers
    if (!set.reps || !set.weight) {
        return std::nullopt;
    }
    // 2. the formula only makes sense for positive reps and weight
    if (*set.reps <= 0 || *set.weight <= 0.0) {
        return std::nullopt;
    }
    // 3. Epley: weight * (1 + reps / 30)
    return *set.weight * (1.0 + *set.reps / 30.0);
}

std::optional<std::chrono::sys_days> ProgressService::parse_iso_date(const std::string& text) {
    int year = 0;
    int month = 0;
    int day = 0;
    char dash1 = 0;
    char dash2 = 0;

    std::istringstream in(text);
    in >> year >> dash1 >> month >> dash2 >> day;
    if (in.fail() || dash1 != '-' || dash2 != '-') {
        return std::nullopt;
    }

    const std::chrono::year_month_day ymd{std::chrono::year{year},
                                          std::chrono::month{static_cast<unsigned>(month)},
                                          std::chrono::day{static_cast<unsigned>(day)}};
    if (!ymd.ok()) {
        return std::nullopt;
    }
    return std::chrono::sys_days{ymd};
}

std::chrono::sys_days ProgressService::monday_of_week(std::chrono::sys_days day) {
    const std::chrono::weekday wd{day};
    return day - (wd - std::chrono::Monday);
}

int ProgressService::weekly_streak(const std::vector<LoggedSet>& sets, const std::string& as_of) {
    const std::optional<std::chrono::sys_days> today = parse_iso_date(as_of);
    if (!today) {
        return 0;
    }

    std::set<std::chrono::sys_days> trained_weeks;
    for (const LoggedSet& set : sets) {
        const std::optional<std::chrono::sys_days> day = parse_iso_date(set.performed_on);
        if (day) {
            trained_weeks.insert(monday_of_week(*day));
        }
    }

    int streak = 0;
    std::chrono::sys_days week = monday_of_week(*today);
    while (trained_weeks.contains(week)) {
        ++streak;
        week -= std::chrono::weeks{1};
    }
    return streak;
}

}  // namespace fitplan::services