#pragma once

#include "app/App.hpp"
#include "repositories/CoachTraineeRepository.hpp"
#include "repositories/UserRepository.hpp"

namespace fitplan::controllers {

// Registers the coach-only roster endpoints on `app`:
//   GET  /api/trainees    list the caller's roster
//   POST /api/trainees    attach a trainee by email
//
// Both require role "coach". Attaching resolves the email through UserRepository,
// checks the account is a trainee, then links it via CoachTraineeRepository.
void register_trainee_routes(app::FitPlanApp& app,
                             repositories::UserRepository& users,
                             repositories::CoachTraineeRepository& roster);

}  // namespace fitplan::controllers
