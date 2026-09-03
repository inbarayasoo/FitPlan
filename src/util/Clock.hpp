#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace fitplan::util {

// A source of "now" as a UTC timestamp string in SQLite's datetime() format,
// "YYYY-MM-DD HH:MM:SS". Injected into services so tests can freeze or advance
// time instead of sleeping. The live implementation is iso_utc_now.
using Clock = std::function<std::string()>;

// Current UTC time as "YYYY-MM-DD HH:MM:SS".
std::string iso_utc_now();

// `iso` shifted by `seconds` (which may be negative), returned in the same
// format. Throws std::invalid_argument if `iso` is not a well-formed
// "YYYY-MM-DD HH:MM:SS" string.
//
// Because the format is fixed-width UTC, lexical string order on these values
// equals chronological order - callers compare them with plain <, >, ==.
std::string iso_utc_shift(const std::string& iso, std::int64_t seconds);

}  // namespace fitplan::util
