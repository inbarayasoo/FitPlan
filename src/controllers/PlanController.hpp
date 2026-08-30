#pragma once

#include "app/App.hpp"
#include "services/PlanService.hpp"

namespace fitplan::controllers {

// Registers the coach-only workout-plan endpoints on `app`:
//   POST /api/plans                 create (header + nested items)
//   GET  /api/plans                 list the caller's plans (headers only)
//   GET  /api/plans/<int>           one plan with its items
//   PUT  /api/plans/<int>           replace header + items
//   POST /api/plans/<int>/assign    make it the trainee's active plan
//
// Every route requires role "coach"; PlanService scopes every call to the
// caller's coach_id.
void register_plan_routes(app::FitPlanApp& app, services::PlanService& plans);

}  // namespace fitplan::controllers
