#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace fitplan::models {

// Mirrors one row of the `exercises` table. Nullable SQL columns map to
// std::optional; a missing value is std::nullopt.
struct Exercise {
    std::int64_t id = 0;
    std::int64_t coach_id = 0;
    std::string name;
    std::optional<std::string> category;
    std::optional<std::string> primary_muscle;
    std::optional<std::string> description;
    std::optional<std::string> video_url;
    std::string created_at;
};

}  // namespace fitplan::models
