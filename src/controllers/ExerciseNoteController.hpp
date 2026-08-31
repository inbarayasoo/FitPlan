#pragma once

#include "app/App.hpp"
#include "repositories/ExerciseNoteRepository.hpp"
#include "repositories/PlanItemRepository.hpp"
#include "repositories/PlanRepository.hpp"

namespace fitplan::controllers {

// Registers the trainee-only per-exercise note endpoints on `app`:
//   GET    /api/my/notes           list the caller's notes
//   PUT    /api/my/notes/<id>      create or overwrite the note for exercise <id>
//   DELETE /api/my/notes/<id>      remove the note for exercise <id>
//
// All require role "trainee". PUT checks the exercise is on the caller's active
// plan (via PlanRepository + PlanItemRepository) before upserting.
void register_exercise_note_routes(app::FitPlanApp& app,
                                   repositories::ExerciseNoteRepository& notes,
                                   repositories::PlanRepository& plans,
                                   repositories::PlanItemRepository& plan_items);

}  // namespace fitplan::controllers
