#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <cstdint>
#include <vector>

#include "models/SessionSet.hpp"

namespace fitplan::repositories {

// Data access for the `session_sets` table - one row per set a trainee logged
// inside a session. Mirrors PlanItemRepository: no stand-alone update, a
// SessionService rewrites a session's sets by delete_by_session + re-insert.
class SessionSetRepository {
public:
    explicit SessionSetRepository(SQLite::Database& db) : db_(db) {}

    // Inserts one set. Uses every field of `set` except id. Returns it with the
    // database-assigned id filled in.
    models::SessionSet create(const models::SessionSet& set);

    // All sets of a session, in logging order (set_number, then id).
    std::vector<models::SessionSet> list_by_session(std::int64_t session_id);

    // Removes every set of a session. Returns the number of rows deleted.
    int delete_by_session(std::int64_t session_id);

private:
    SQLite::Database& db_;
};

}  // namespace fitplan::repositories
