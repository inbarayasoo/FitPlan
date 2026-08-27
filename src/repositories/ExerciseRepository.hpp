#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "models/Exercise.hpp"

namespace fitplan::repositories {

// Data access for the `exercises` table. All SQL for exercises lives here and
// nowhere else. Holds a reference to the connection - it does not own it, the
// Database object does - so the connection must outlive the repository.
class ExerciseRepository {
public:
    explicit ExerciseRepository(SQLite::Database& db) : db_(db) {}

    // Inserts a new row. Uses e.coach_id, e.name, e.category, e.primary_muscle,
    // e.description, e.video_url. Returns the stored exercise with its
    // database-assigned id and created_at filled in.
    models::Exercise create(const models::Exercise& e);

    // Returns the exercise with this id, or std::nullopt if there is no such row.
    std::optional<models::Exercise> find_by_id(std::int64_t id);

    // All exercises owned by this coach, newest first.
    std::vector<models::Exercise> list_by_coach(std::int64_t coach_id);

    // Updates the mutable columns (name, category, primary_muscle, description,
    // video_url) of the row with e.id. Returns true if a row was changed.
    bool update(const models::Exercise& e);

    // Deletes the row with this id. Returns true if a row was removed.
    bool remove(std::int64_t id);

private:
    SQLite::Database& db_;
};

}  // namespace fitplan::repositories
