#pragma once

#include <crow.h>

#include "middleware/JwtAuthMiddleware.hpp"
#include "middleware/RequestLogger.hpp"

namespace fitplan::app {

// The one concrete Crow application type for the whole server. main.cpp builds
// it and the controllers take it by reference, so route registration and
// get_context<>() always agree on which middleware are in the stack.
//
// Order = outer to inner: RequestLogger wraps everything (it times the whole
// request, JWT parsing included); JwtAuthMiddleware runs next and fills the auth
// context the routes read.
using FitPlanApp =
    crow::App<middleware::RequestLogger, middleware::JwtAuthMiddleware>;

}  // namespace fitplan::app
