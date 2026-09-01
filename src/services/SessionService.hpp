#pragma once

#include <SQLiteCpp/SQLiteCpp.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "models/SessionSet.hpp"
#include "models/WorkoutSession.hpp"
#include "repositories/ExerciseRepository.hpp"
#include "repositories/PlanItemRepository.hpp"
#include "repositories/PlanRepository.hpp"
#include "repositories/SessionRepository.hpp"
#include "repositories/SessionSetRepository.hpp"
#include "services/PlanService.hpp"      // services::PlanWithItems
#include "services/ProgressService.hpp"  // services::LoggedSet, PrescribedItem

namespace fitplan::services {

// One set as it arrives from the trainee. set_number is NOT here - the service
// assigns it from list position, like PlanService does for order_index.
struct SessionSetInput {
    std::int64_t exercise_id = 0;
    std::optional<std::int64_t> plan_item_id;
    std::optional<int> reps;
    std::optional<double> weight;
    std::optional<double> rpe;
    bool completed = true;
};

// A whole logged workout as the trainee wants it stored: a header plus its sets.
struct SessionInput {
    std::optional<std::int64_t> plan_id;
    std::optional<std::string> performed_at;  // empty/absent => default to now
    std::string status = "completed";
    std::optional<std::string> notes;
    std::vector<SessionSetInput> sets;
};

// A PATCH on a session: only the fields the caller sent. `set_notes` tells the
// service the caller included "notes" (possibly null) versus omitted it.
// `set_sets` likewise: when true, `sets` replaces the session's whole set list.
struct SessionPatch {
    std::optional<std::string> status;
    bool set_notes = false;
    std::optional<std::string> notes;
    bool set_sets = false;
    std::vector<SessionSetInput> sets;
};

// What every session read/write returns: the stored header and its stored sets.
struct SessionWithSets {
    models::WorkoutSession session;
    std::vector<models::SessionSet> sets;
};

// Trainee-side business logic: viewing the active plan, logging sessions, and
// producing the flattened inputs ProgressService consumes. Crow-free. Takes the
// connection so log_session can wrap the header + N sets in one transaction.
class SessionService {
public:
    SessionService(SQLite::Database& db, repositories::SessionRepository& sessions,
                   repositories::SessionSetRepository& session_sets,
                   repositories::PlanRepository& plans,
                   repositories::PlanItemRepository& plan_items,
                   repositories::ExerciseRepository& exercises)
        : db_(db),
          sessions_(sessions),
          session_sets_(session_sets),
          plans_(plans),
          plan_items_(plan_items),
          exercises_(exercises) {}

    // The trainee's active plan with its items. Each item's video_url is the
    // effective link: its own override if set, else the exercise's library link.
    // Throws SessionError(kNotFound) if no plan is active.
    PlanWithItems active_plan_for(std::int64_t trainee_id);

    // Create a session and its sets in one transaction. Throws SessionError:
    //   kInvalidInput - bad status, unknown exercise_id, a negative number
    //   kForbidden    - a set links a plan item that is not on the active plan
    SessionWithSets log_session(std::int64_t trainee_id, const SessionInput& in);

    // All of the trainee's sessions, newest first, each with its sets.
    std::vector<SessionWithSets> list_sessions(std::int64_t trainee_id);

    // One session with its sets. Throws SessionError(kNotFound) when it is
    // missing or belongs to another trainee.
    SessionWithSets get_session(std::int64_t trainee_id, std::int64_t session_id);

    // Apply a PATCH to the trainee's session. Throws SessionError
    // kNotFound / kInvalidInput / kForbidden.
    SessionWithSets update_session(std::int64_t trainee_id, std::int64_t session_id,
                                   const SessionPatch& patch);

    // Delete the trainee's session (its sets cascade). Throws
    // SessionError(kNotFound) if it is missing or belongs to another trainee.
    void delete_session(std::int64_t trainee_id, std::int64_t session_id);

    // --- flattened inputs for ProgressService -----------------------------
    // Every logged set the trainee has, each tagged with its session's date.
    std::vector<LoggedSet> logged_sets_for(std::int64_t trainee_id);
    // The active plan's items as (plan_item_id, target_sets); empty if no plan.
    std::vector<PrescribedItem> prescribed_for(std::int64_t trainee_id);

private:
    models::WorkoutSession owned_session_or_throw(std::int64_t trainee_id, std::int64_t session_id);
    // Validate a set list the trainee sent: known exercise_id, non-negative
    // numbers, and any plan_item_id must sit on the trainee's active plan.
    void validate_sets(std::int64_t trainee_id, const std::vector<SessionSetInput>& sets);
    // Clear `session_id`'s existing sets and write `sets` in order, numbering
    // from 1. Assumes the caller opened a transaction.
    void write_sets(std::int64_t session_id, const std::vector<SessionSetInput>& sets);
    static void check_status(const std::string& status);
    static std::string date_of(const std::string& timestamp);

    SQLite::Database& db_;
    repositories::SessionRepository& sessions_;
    repositories::SessionSetRepository& session_sets_;
    repositories::PlanRepository& plans_;
    repositories::PlanItemRepository& plan_items_;
    repositories::ExerciseRepository& exercises_;
};

}  // namespace fitplan::services
