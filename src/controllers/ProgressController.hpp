#pragma once

#include "app/App.hpp"
#include "repositories/CoachTraineeRepository.hpp"
#include "repositories/ExerciseRepository.hpp"
#include "services/SessionService.hpp"

namespace fitplan::controllers {

// Registers the progress endpoints on `app`:
//   GET /api/my/progress              trainee's own progress (role "trainee")
//   GET /api/trainees/<int>/progress  a coach's view, limited to the roster
//                                     (role "coach"; 404 if not linked)
//
// Both assemble ProgressService's pure functions over the trainee's logged sets
// and active-plan targets.
void register_progress_routes(app::FitPlanApp& app, services::SessionService& sessions,
                              repositories::CoachTraineeRepository& roster,
                              repositories::ExerciseRepository& exercises);

}  // namespace fitplan::controllers
