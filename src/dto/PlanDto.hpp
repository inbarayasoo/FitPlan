#pragma once

#include <crow.h>

#include <string>
#include <vector>

#include "models/WorkoutPlan.hpp"
#include "services/PlanService.hpp"

namespace fitplan::dto {

// Parse + shallow-validate the body of POST /api/plans and PUT /api/plans/{id}.
// Produces the Crow-free input struct the service expects. Throws
// http::ApiError(kInvalidInput) on malformed JSON, a missing / blank name, a
// missing / empty items array, a non-integer exercise_id, a wrong-typed optional
// field, or a per-item video_url that is not an allowed link. Deeper rules
// (roster membership, exercise ownership, positive targets) are the service's.
services::PlanInput parse_plan_request(const std::string& body);

// One plan with its items as JSON. Item objects carry a computed
// `video_embed_url` just like the exercise DTO does.
crow::response plan_response(int status, const services::PlanWithItems& plan);

// { "plans": [ <header>, ... ] } - headers only, no items.
crow::response plan_list_response(const std::vector<models::WorkoutPlan>& plans);

}  // namespace fitplan::dto
