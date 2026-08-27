#pragma once

#include <cstdint>
#include <string>

namespace fitplan::models {

// Mirrors one row of the `users` table. Plain data: no behaviour, no invariants
// enforced here - validation lives in the service layer.
struct User {
    std::int64_t id = 0;
    std::string email;
    std::string password_hash;
    std::string role;  // "trainee" | "coach"
    std::string display_name;
    std::string created_at;
};

}  // namespace fitplan::models
