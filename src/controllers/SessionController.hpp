#pragma once

#include "app/App.hpp"
#include "services/SessionService.hpp"

namespace fitplan::controllers {

// Registers the trainee-only session endpoints on `app`:
//   GET   /api/my/plan              the trainee's active plan (effective video)
//   GET   /api/my/sessions          list logged sessions, newest first
//   POST  /api/my/sessions          log a session + its sets in one payload
//   PATCH /api/my/sessions/<int>    update a session's status / notes
//
// All require role "trainee". The caller's id comes from the JWT, never the body.
void register_session_routes(app::FitPlanApp& app, services::SessionService& sessions);

}  // namespace fitplan::controllers
