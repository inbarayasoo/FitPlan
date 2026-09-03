#pragma once

#include "app/App.hpp"
#include "repositories/CoachTraineeRepository.hpp"
#include "services/SessionService.hpp"

namespace fitplan::controllers {

// Registers the session endpoints on `app`:
//   GET   /api/my/plan                  the trainee's active plan (effective video)
//   GET   /api/my/sessions              list logged sessions, newest first
//   POST  /api/my/sessions              log a session + its sets in one payload
//   PATCH /api/my/sessions/<int>        update a session's status and/or set list
//   DELETE /api/my/sessions/<int>       delete a session
//   GET   /api/trainees/<int>/sessions  a coach's read-only view of ONE roster
//                                       trainee's log (same gate as .../progress)
//
// The /my/* routes require role "trainee"; /api/trainees/<int>/sessions requires
// role "coach". The caller's id always comes from the JWT, never the body.
void register_session_routes(app::FitPlanApp& app, services::SessionService& sessions,
                             repositories::CoachTraineeRepository& roster);

}  // namespace fitplan::controllers
