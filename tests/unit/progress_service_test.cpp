// Table-driven unit tests for ProgressService::total_volume.

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

#include "services/ProgressService.hpp"
#include <utility>

namespace {

using fitplan::services::LoggedSet;
using fitplan::services::ProgressService;
using fitplan::services::SeriesPoint;

// A completed set with reps and weight filled in.
LoggedSet done(int reps, double weight) {
    LoggedSet s;
    s.performed_on = "2026-08-30";
    s.reps = reps;
    s.weight = weight;
    s.completed = true;
    return s;
}

TEST(TotalVolume, SumsRepsTimesWeightOverCountedSets) {
    struct Case {
        std::string name;
        std::vector<LoggedSet> sets;
        double expected;
    };

    LoggedSet no_reps = done(0, 100.0);
    no_reps.reps = std::nullopt;

    LoggedSet not_completed = done(5, 80.0);
    not_completed.completed = false;

    const std::vector<Case> cases = {
        {"empty list is zero", {}, 0.0},
        {"one set", {done(10, 20.0)}, 200.0},
        {"two sets add up", {done(10, 20.0), done(5, 40.0)}, 400.0},
        {"missing reps is skipped", {done(10, 20.0), no_reps}, 200.0},
        {"non-completed set is skipped", {done(10, 20.0), not_completed}, 200.0},
        {"zero reps contributes nothing", {done(0, 50.0)}, 0.0},
    };

    for (const Case& c : cases) {
        EXPECT_DOUBLE_EQ(ProgressService::total_volume(c.sets), c.expected)
            << "case: " << c.name;
    }
}

TEST(Epley1rm, ReturnsNulloptOrTheFormulaResult) {
    struct Case {
        std::string name;
        std::optional<int> reps;
        std::optional<double> weight;
        std::optional<double> expected;  // nullopt => function returns nullopt
    };

    const std::vector<Case> cases = {
        {"both missing", std::nullopt, std::nullopt, std::nullopt},
        {"reps missing", std::nullopt, 100.0, std::nullopt},
        {"weight missing", 10, std::nullopt, std::nullopt},
        {"zero reps", 0, 100.0, std::nullopt},
        {"zero weight", 10, 0.0, std::nullopt},
        {"negative reps", -5, 100.0, std::nullopt},
        {"ten reps at 100", 10, 100.0, 100.0 * (1.0 + 10.0 / 30.0)},
        {"thirty reps doubles the weight", 30, 100.0, 200.0},
        {"one rep", 1, 60.0, 60.0 * (1.0 + 1.0 / 30.0)},
    };

    for (const Case& c : cases) {
        LoggedSet s;
        s.reps = c.reps;
        s.weight = c.weight;

        const std::optional<double> got = ProgressService::epley_1rm(s);

        ASSERT_EQ(got.has_value(), c.expected.has_value()) << "case: " << c.name;
        if (c.expected) {
            EXPECT_DOUBLE_EQ(*got, *c.expected) << "case: " << c.name;
        }
    }
}

// completed is ignored by epley_1rm; the caller decides whether a set counts.
TEST(Epley1rm, DoesNotLookAtCompleted) {
    LoggedSet failed_attempt;
    failed_attempt.reps = 5;
    failed_attempt.weight = 120.0;
    failed_attempt.completed = false;

    EXPECT_TRUE(ProgressService::epley_1rm(failed_attempt).has_value());
}

TEST(VolumeOverTime, GroupsByDayAscendingAndSkipsEmptyDays) {
    auto on = [](std::string day, int reps, double weight, bool completed = true) {
        LoggedSet s;
        s.performed_on = std::move(day);
        s.reps = reps;
        s.weight = weight;
        s.completed = completed;
        return s;
    };

    struct Case {
        std::string name;
        std::vector<LoggedSet> sets;
        std::vector<std::pair<std::string, double>> expected;
    };

    const std::vector<Case> cases = {
        {"empty list", {}, {}},
        {"one day, two sets add up",
         {on("2026-08-10", 10, 20.0), on("2026-08-10", 5, 40.0)},
         {{"2026-08-10", 400.0}}},
        {"days come back sorted regardless of input order",
         {on("2026-08-12", 1, 100.0), on("2026-08-10", 1, 50.0)},
         {{"2026-08-10", 50.0}, {"2026-08-12", 100.0}}},
        {"a day with only skipped sets is absent",
         {on("2026-08-10", 10, 20.0), on("2026-08-11", 5, 40.0, false)},
         {{"2026-08-10", 200.0}}},
    };

    for (const Case& c : cases) {
        const std::vector<SeriesPoint> got =
            ProgressService::volume_over_time(c.sets);

        ASSERT_EQ(got.size(), c.expected.size()) << "case: " << c.name;
        for (std::size_t i = 0; i < got.size(); ++i) {
            EXPECT_EQ(got[i].date, c.expected[i].first) << "case: " << c.name;
            EXPECT_DOUBLE_EQ(got[i].value, c.expected[i].second)
                << "case: " << c.name;
        }
    }
}

TEST(BestE1rmOverTime, KeepsDailyMaxForOneExerciseSortedByDate) {
    auto set_for = [](std::int64_t ex, std::string day, std::optional<int> reps,
                      std::optional<double> weight, bool completed = true) {
        LoggedSet s;
        s.exercise_id = ex;
        s.performed_on = std::move(day);
        s.reps = reps;
        s.weight = weight;
        s.completed = completed;
        return s;
    };

    const std::int64_t squat = 1;
    const std::int64_t bench = 2;

    const std::vector<LoggedSet> sets = {
        set_for(squat, "2026-08-10", 5, 100.0),             // lower estimate
        set_for(squat, "2026-08-10", 3, 110.0),             // this day's best
        set_for(squat, "2026-08-10", 8, 90.0, false),       // not completed
        set_for(squat, "2026-08-12", 1, 130.0),             // only valid set that day
        set_for(squat, "2026-08-12", std::nullopt, 140.0),  // no reps -> skipped
        set_for(bench, "2026-08-11", 5, 80.0),              // different exercise
    };

    const std::vector<SeriesPoint> got =
        ProgressService::best_e1rm_over_time(sets, squat);

    ASSERT_EQ(got.size(), 2u);
    EXPECT_EQ(got[0].date, "2026-08-10");
    EXPECT_DOUBLE_EQ(got[0].value, 110.0 * (1.0 + 3.0 / 30.0));
    EXPECT_EQ(got[1].date, "2026-08-12");
    EXPECT_DOUBLE_EQ(got[1].value, 130.0 * (1.0 + 1.0 / 30.0));
}

TEST(BestE1rmOverTime, EmptyWhenNoSetMatchesTheExercise) {
    LoggedSet other;
    other.exercise_id = 99;
    other.performed_on = "2026-08-10";
    other.reps = 5;
    other.weight = 100.0;

    EXPECT_TRUE(ProgressService::best_e1rm_over_time({other}, 1).empty());
}

TEST(Adherence, RatioOfCompletedPrescribedSetsClampedToOne) {
    using fitplan::services::PrescribedItem;

    auto logged = [](std::optional<std::int64_t> plan_item_id, bool completed) {
        LoggedSet s;
        s.performed_on = "2026-08-10";
        s.reps = 5;
        s.weight = 50.0;
        s.plan_item_id = plan_item_id;
        s.completed = completed;
        return s;
    };

    // plan asks for 3 sets of item 1 and 1 set of item 2 -> 4 prescribed
    const std::vector<PrescribedItem> plan = {{1, 3}, {2, 1}};

    struct Case {
        std::string name;
        std::vector<PrescribedItem> prescribed;
        std::vector<LoggedSet> sets;
        double expected;
    };

    const std::vector<Case> cases = {
        {"nothing prescribed is zero", {}, {logged(1, true), logged(1, true)}, 0.0},
        {"none done is zero", plan, {}, 0.0},
        {"all four done is one",
         plan,
         {logged(1, true), logged(1, true), logged(1, true), logged(2, true)},
         1.0},
        {"two of four done is a half", plan, {logged(1, true), logged(2, true)}, 0.5},
        {"uncompleted sets do not count",
         plan,
         {logged(1, true), logged(1, false), logged(1, false), logged(2, false)},
         0.25},
        {"sets with no plan item do not count",
         plan,
         {logged(1, true), logged(std::nullopt, true)},
         0.25},
        {"sets for an unknown item do not count",
         plan,
         {logged(1, true), logged(99, true)},
         0.25},
        {"more than prescribed is clamped to one",
         plan,
         {logged(1, true), logged(1, true), logged(1, true), logged(1, true),
          logged(1, true), logged(2, true)},
         1.0},
        {"item with a zero target is left out of the denominator",
         {{1, 2}, {2, 0}},
         {logged(1, true), logged(2, true)},
         0.5},
    };

    for (const Case& c : cases) {
        EXPECT_DOUBLE_EQ(ProgressService::adherence(c.sets, c.prescribed),
                         c.expected)
            << "case: " << c.name;
    }
}

TEST(WeeklyStreak, CountsConsecutiveWeeksBackFromAsOf) {
    auto on_day = [](std::string day) {
        LoggedSet s;
        s.performed_on = std::move(day);
        s.reps = 5;
        s.weight = 50.0;
        return s;
    };

    struct Case {
        std::string name;
        std::vector<LoggedSet> sets;
        std::string as_of;
        int expected;
    };

    // Weeks start on Monday. 2026-08-03 is a Monday, so:
    //   week B: 2026-08-10..16   week C: 2026-08-17..23
    //   week D: 2026-08-24..30   week E: 2026-08-31..09-06
    const std::vector<Case> cases = {
        {"no sets", {}, "2026-08-30", 0},
        {"nothing in the as_of week",
         {on_day("2026-08-18"), on_day("2026-08-12")}, "2026-08-30", 0},
        {"one week only", {on_day("2026-08-24")}, "2026-08-30", 1},
        {"two sets in the same week count once",
         {on_day("2026-08-24"), on_day("2026-08-30")}, "2026-08-30", 1},
        {"three consecutive weeks",
         {on_day("2026-08-30"), on_day("2026-08-20"), on_day("2026-08-11")},
         "2026-08-30", 3},
        {"a missing week breaks the run",
         {on_day("2026-08-30"), on_day("2026-08-11")}, "2026-08-30", 1},
        {"weeks after as_of do not count",
         {on_day("2026-09-02"), on_day("2026-08-30")}, "2026-08-30", 1},
        {"unparseable dates are ignored",
         {on_day("not-a-date"), on_day("2026-08-30")}, "2026-08-30", 1},
        {"an unparseable as_of is zero",
         {on_day("2026-08-30")}, "nope", 0},
    };

    for (const Case& c : cases) {
        EXPECT_EQ(ProgressService::weekly_streak(c.sets, c.as_of), c.expected)
            << "case: " << c.name;
    }
}

}  // namespace