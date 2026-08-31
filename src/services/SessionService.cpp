#include "services/SessionService.hpp"

#include <set>
#include <string>
#include <utility>

#include "models/Exercise.hpp"
#include "models/PlanItem.hpp"
#include "models/WorkoutPlan.hpp"
#include "services/SessionError.hpp"

namespace fitplan::services {

void SessionService::check_status(const std::string& status) {
    static const std::set<std::string> kStatuses = {"planned", "in_progress",
                                                    "completed"};
    if (kStatuses.count(status) == 0) {
        throw SessionError(SessionErrorKind::kInvalidInput,
                           "status must be planned, in_progress, or completed");
    }
}

std::string SessionService::date_of(const std::string& timestamp) {
    // performed_at is "YYYY-MM-DD HH:MM:SS"; ProgressService wants just the day.
    return timestamp.substr(0, 10);
}

models::WorkoutSession SessionService::owned_session_or_throw(
    std::int64_t trainee_id, std::int64_t session_id) {
    const std::optional<models::WorkoutSession> found =
        sessions_.find_by_id(session_id);
    if (!found || found->trainee_id != trainee_id) {
        throw SessionError(SessionErrorKind::kNotFound,
                           "no session with that id");
    }
    return *found;
}

PlanWithItems SessionService::active_plan_for(std::int64_t trainee_id) {
    const std::optional<models::WorkoutPlan> plan =
        plans_.find_active_for_trainee(trainee_id);
    if (!plan) {
        throw SessionError(SessionErrorKind::kNotFound,
                           "you have no active plan");
    }

    std::vector<models::PlanItem> items = plan_items_.list_by_plan(plan->id);
    for (models::PlanItem& item : items) {
        if (!item.video_url) {
            if (const std::optional<models::Exercise> ex =
                    exercises_.find_by_id(item.exercise_id)) {
                item.video_url = ex->video_url;  // may itself be std::nullopt
            }
        }
    }
    return PlanWithItems{*plan, std::move(items)};
}

void SessionService::validate_sets(std::int64_t trainee_id,
                                   const std::vector<SessionSetInput>& sets) {
    // plan_item_ids that belong to the trainee's active plan; a per-set link is
    // only allowed to point at one of these.
    std::set<std::int64_t> active_items;
    if (const std::optional<models::WorkoutPlan> plan =
            plans_.find_active_for_trainee(trainee_id)) {
        for (const models::PlanItem& it : plan_items_.list_by_plan(plan->id)) {
            active_items.insert(it.id);
        }
    }

    for (const SessionSetInput& s : sets) {
        if (!exercises_.find_by_id(s.exercise_id)) {
            throw SessionError(SessionErrorKind::kInvalidInput,
                               "a set references an unknown exercise_id");
        }
        if (s.plan_item_id && active_items.count(*s.plan_item_id) == 0) {
            throw SessionError(
                SessionErrorKind::kForbidden,
                "a set references a plan item that is not on your active plan");
        }
        if (s.reps && *s.reps < 0) {
            throw SessionError(SessionErrorKind::kInvalidInput,
                               "reps must not be negative");
        }
        if (s.weight && *s.weight < 0.0) {
            throw SessionError(SessionErrorKind::kInvalidInput,
                               "weight must not be negative");
        }
        if (s.rpe && (*s.rpe < 0.0 || *s.rpe > 10.0)) {
            throw SessionError(SessionErrorKind::kInvalidInput,
                               "rpe must be between 0 and 10");
        }
    }
}

void SessionService::write_sets(std::int64_t session_id,
                                const std::vector<SessionSetInput>& sets) {
    session_sets_.delete_by_session(session_id);
    int set_number = 1;
    for (const SessionSetInput& s : sets) {
        models::SessionSet row;
        row.session_id = session_id;
        row.exercise_id = s.exercise_id;
        row.plan_item_id = s.plan_item_id;
        row.set_number = set_number++;
        row.reps = s.reps;
        row.weight = s.weight;
        row.rpe = s.rpe;
        row.completed = s.completed;
        session_sets_.create(row);
    }
}

SessionWithSets SessionService::log_session(std::int64_t trainee_id,
                                            const SessionInput& in) {
    check_status(in.status);
    validate_sets(trainee_id, in.sets);

    SQLite::Transaction tx(db_);

    models::WorkoutSession header;
    header.trainee_id = trainee_id;
    header.plan_id = in.plan_id;
    header.performed_at = in.performed_at.value_or("");
    header.status = in.status;
    header.notes = in.notes;
    const models::WorkoutSession saved = sessions_.create(header);

    write_sets(saved.id, in.sets);

    tx.commit();

    return SessionWithSets{sessions_.find_by_id(saved.id).value(),
                           session_sets_.list_by_session(saved.id)};
}

std::vector<SessionWithSets> SessionService::list_sessions(
    std::int64_t trainee_id) {
    std::vector<SessionWithSets> out;
    for (const models::WorkoutSession& s : sessions_.list_by_trainee(trainee_id)) {
        out.push_back({s, session_sets_.list_by_session(s.id)});
    }
    return out;
}

SessionWithSets SessionService::get_session(std::int64_t trainee_id,
                                            std::int64_t session_id) {
    const models::WorkoutSession s =
        owned_session_or_throw(trainee_id, session_id);
    return SessionWithSets{s, session_sets_.list_by_session(session_id)};
}

SessionWithSets SessionService::update_session(std::int64_t trainee_id,
                                              std::int64_t session_id,
                                              const SessionPatch& patch) {
    models::WorkoutSession session =
        owned_session_or_throw(trainee_id, session_id);

    if (patch.status) {
        check_status(*patch.status);
        session.status = *patch.status;
    }
    if (patch.set_notes) {
        session.notes = patch.notes;
    }
    if (patch.set_sets) {
        validate_sets(trainee_id, patch.sets);
    }

    SQLite::Transaction tx(db_);
    sessions_.update(session);
    if (patch.set_sets) {
        write_sets(session_id, patch.sets);
    }
    tx.commit();

    return SessionWithSets{sessions_.find_by_id(session_id).value(),
                           session_sets_.list_by_session(session_id)};
}

void SessionService::delete_session(std::int64_t trainee_id,
                                    std::int64_t session_id) {
    owned_session_or_throw(trainee_id, session_id);
    sessions_.remove(session_id);
}

std::vector<LoggedSet> SessionService::logged_sets_for(std::int64_t trainee_id) {
    std::vector<LoggedSet> out;
    for (const models::WorkoutSession& s :
         sessions_.list_by_trainee(trainee_id)) {
        const std::string day = date_of(s.performed_at);
        for (const models::SessionSet& set : session_sets_.list_by_session(s.id)) {
            LoggedSet ls;
            ls.exercise_id = set.exercise_id;
            ls.performed_on = day;
            ls.reps = set.reps;
            ls.weight = set.weight;
            ls.completed = set.completed;
            ls.plan_item_id = set.plan_item_id;
            out.push_back(ls);
        }
    }
    return out;
}

std::vector<PrescribedItem> SessionService::prescribed_for(
    std::int64_t trainee_id) {
    std::vector<PrescribedItem> out;
    const std::optional<models::WorkoutPlan> plan =
        plans_.find_active_for_trainee(trainee_id);
    if (!plan) {
        return out;
    }
    for (const models::PlanItem& item : plan_items_.list_by_plan(plan->id)) {
        PrescribedItem p;
        p.plan_item_id = item.id;
        p.target_sets = item.target_sets.value_or(0);
        out.push_back(p);
    }
    return out;
}

}  // namespace fitplan::services
