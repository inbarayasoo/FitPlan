#pragma once

#include <string>

#include "app/App.hpp"
#include "services/AuthService.hpp"

namespace fitplan::controllers {

// Registers the auth routes on `app`, wiring each to `auth`:
//   POST /api/auth/register              (201; emails a code, issues no token)
//   POST /api/auth/verify-email          (check the code -> { access_token, user })
//   POST /api/auth/resend-verification   (202; always silent about the address)
//   POST /api/auth/login, GET /api/auth/me
//   POST /api/auth/google    (exchange a Google ID token for a FitPlan token;
//                             400 when Google Sign-In is not configured)
//   GET  /api/auth/config    (reports `google_client_id` so the frontend knows
//                             whether to show the Google button)
// This is the only auth code that mentions HTTP.
void register_auth_routes(app::FitPlanApp& app, services::AuthService& auth,
                          const std::string& google_client_id = "");

}  // namespace fitplan::controllers
