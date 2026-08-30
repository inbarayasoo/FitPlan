#pragma once

#include "app/App.hpp"
#include "repositories/ExerciseRepository.hpp"

namespace fitplan::controllers {

// Registers the coach-only exercise-library endpoints on `app`:
//   POST   /api/exercises          create
//   GET    /api/exercises          list (only the caller's own)
//   GET    /api/exercises/<int>    fetch one (404 if not the caller's)
//   PUT    /api/exercises/<int>    update
//   DELETE /api/exercises/<int>    delete -> 204
//
// Every route requires a valid token with role "coach"; each row is scoped to
// the coach that owns it. The controller talks straight to the repository -
// there is no ExerciseService, because there are no cross-row rules here.
void register_exercise_routes(app::FitPlanApp& app,
                              repositories::ExerciseRepository& exercises);

}  // namespace fitplan::controllers
