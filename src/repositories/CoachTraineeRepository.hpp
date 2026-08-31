#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <cstdint>
#include <vector>

#include "models/User.hpp"

namespace fitplan::repositories {

// Data access for the `coach_trainees` join table - a coach's roster. Attaching
// a trainee by email is a two-step job: UserRepository::find_by_email() to turn
// the email into a user, then link() here.
class CoachTraineeRepository {
public:
    explicit CoachTraineeRepository(SQLite::Database& db) : db_(db) {}

    // Records that `trainee_id` is on `coach_id`'s roster. Returns true if this
    // created a new link, false if the pair was already linked (INSERT OR
    // IGNORE - the primary key is the pair).
    bool link(std::int64_t coach_id, std::int64_t trainee_id);

    // True if the pair is already on the roster. Used for the 409 on a repeat
    // attach, and for the ownership check when a coach builds a plan.
    bool is_linked(std::int64_t coach_id, std::int64_t trainee_id);

    // Removes `trainee_id` from `coach_id`'s roster. Returns true if a link was
    // deleted, false if the pair was not on the roster. Existing workout plans
    // are left untouched.
    bool unlink(std::int64_t coach_id, std::int64_t trainee_id);

    // Every trainee on this coach's roster, most recently added first.
    std::vector<models::User> list_trainees(std::int64_t coach_id);

private:
    SQLite::Database& db_;
};

}  // namespace fitplan::repositories
