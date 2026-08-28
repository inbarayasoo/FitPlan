#pragma once

#include "app/App.hpp"
#include "services/AuthService.hpp"

namespace fitplan::controllers {

// Registers POST /api/auth/register, POST /api/auth/login and GET /api/auth/me
// on `app`, wiring each to `auth`. This is the only auth code that mentions HTTP.
void register_auth_routes(app::FitPlanApp& app, services::AuthService& auth);

}  // namespace fitplan::controllers
