#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "models/ExerciseNote.hpp"

namespace fitplan::repositories {

// Data access for `exercise_notes` - a trainee's per-exercise notes. Every read
// JOINs `exercises` for the name. A note is identified by the (trainee, exercise)
// pair, not its row id, so the API can address it by exercise_id.
class ExerciseNoteRepository {
public:
    explicit ExerciseNoteRepository(SQLite::Database& db) : db_(db) {}

    // Every note this trainee has, newest change first.
    std::vector<models::ExerciseNote> list_for_trainee(std::int64_t trainee_id);

    std::optional<models::ExerciseNote> find(std::int64_t trainee_id, std::int64_t exercise_id);

    // Inserts the note, or overwrites the body of an existing one for the same
    // (trainee, exercise) pair. Returns the stored row.
    models::ExerciseNote upsert(std::int64_t trainee_id, std::int64_t exercise_id,
                                const std::string& body);

    // Removes the note for this pair. Returns true if a row was deleted.
    bool remove(std::int64_t trainee_id, std::int64_t exercise_id);

private:
    SQLite::Database& db_;
};

}  // namespace fitplan::repositories
