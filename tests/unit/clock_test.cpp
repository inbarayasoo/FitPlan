#include "util/Clock.hpp"

#include <gtest/gtest.h>

#include <regex>
#include <stdexcept>
#include <string>

namespace {

using fitplan::util::iso_utc_now;
using fitplan::util::iso_utc_shift;

TEST(ClockTest, IsoUtcNowHasTheFixedFormat) {
    const std::string now = iso_utc_now();
    EXPECT_EQ(now.size(), 19u);
    EXPECT_TRUE(std::regex_match(now, std::regex(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})")));
}

TEST(ClockTest, ShiftAddsAndSubtractsSeconds) {
    EXPECT_EQ(iso_utc_shift("2026-09-02 14:20:00", 600), "2026-09-02 14:30:00");
    EXPECT_EQ(iso_utc_shift("2026-09-02 14:20:00", -1200), "2026-09-02 14:00:00");
    EXPECT_EQ(iso_utc_shift("2026-09-02 14:20:00", 0), "2026-09-02 14:20:00");
}

TEST(ClockTest, ShiftCrossesMinuteHourAndDayBoundaries) {
    EXPECT_EQ(iso_utc_shift("2026-09-02 23:59:30", 60), "2026-09-03 00:00:30");
    EXPECT_EQ(iso_utc_shift("2026-12-31 23:59:59", 1), "2027-01-01 00:00:00");
}

TEST(ClockTest, ShiftRejectsAMalformedTimestamp) {
    EXPECT_THROW(iso_utc_shift("not-a-timestamp", 1), std::invalid_argument);
    EXPECT_THROW(iso_utc_shift("", 1), std::invalid_argument);
}

TEST(ClockTest, LexicalOrderEqualsChronologicalOrder) {
    // The property services rely on: comparing these strings with < is comparing
    // the instants they name.
    const std::string base = "2026-09-02 14:00:00";
    EXPECT_LT(base, iso_utc_shift(base, 1));
    EXPECT_GT(iso_utc_shift(base, 3600), base);
    EXPECT_EQ(iso_utc_shift(base, 0), base);
}

}  // namespace
