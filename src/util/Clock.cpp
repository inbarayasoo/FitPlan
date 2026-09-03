#include "util/Clock.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fitplan::util {

namespace {

std::string format_utc(std::time_t seconds_since_epoch) {
    std::tm tm{};
    ::gmtime_r(&seconds_since_epoch, &tm);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

}  // namespace

std::string iso_utc_now() {
    return format_utc(std::time(nullptr));
}

std::string iso_utc_shift(const std::string& iso, std::int64_t seconds) {
    std::tm tm{};
    std::istringstream in(iso);
    in >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (in.fail()) {
        throw std::invalid_argument("iso_utc_shift: not a 'YYYY-MM-DD HH:MM:SS' timestamp: " + iso);
    }
    // timegm interprets the fields as UTC (mktime would apply the local zone).
    const std::time_t base = ::timegm(&tm);
    return format_utc(base + static_cast<std::time_t>(seconds));
}

}  // namespace fitplan::util
