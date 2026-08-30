#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "models/WorkoutSession.hpp"

namespace fitplan::repositories {

// Data access for the `workout_sessions` table - a workout a trainee actually
// performed. The per-set rows live in SessionSetRepository; a SessionService
// stitches the two together inside one transaction.
class SessionRepository {
public:
    explicit SessionRepository(SQLite::Database& db) : db_(db) {}

    // Inserts one session header. Uses s.trainee_id, s.plan_id, s.performed_at
    // (empty string => let the column default to now), s.status, s.notes.
    // Returns it with the database-assigned id and performed_at filled in.
    models::WorkoutSession create(const models::WorkoutSession& s);

    std::optional<models::WorkoutSession> find_by_id(std::int64_t id);

    // Every session this trainee logged, most recent first.
    std::vector<models::WorkoutSession> list_by_trainee(std::int64_t trainee_id);

    // Updates the editable columns (status, notes) of the row with s.id.
    // Returns true if a row changed.
    bool update(const models::WorkoutSession& s);

private:
    SQLite::Database& db_;
};

}  // namespace fitplan::repositories
