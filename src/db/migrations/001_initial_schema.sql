-- 001_initial_schema.sql
-- The initial FitPlan schema: accounts, the coach roster, the exercise library,
-- workout plans and their items, logged sessions, and per-set results.
--
-- Conventions used throughout:
--   * INTEGER PRIMARY KEY is an alias for SQLite's rowid: auto-assigned, unique.
--   * Timestamps are TEXT in UTC ISO-8601 ("2026-08-27 09:30:00"), via datetime().
--   * Booleans are INTEGER 0/1 with a CHECK constraint.
--   * Every foreign key gets an index (SQLite does not create one automatically).

CREATE TABLE users (
    id            INTEGER PRIMARY KEY,
    email         TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    role          TEXT NOT NULL CHECK (role IN ('trainee', 'coach')),
    display_name  TEXT NOT NULL,
    created_at    TEXT NOT NULL DEFAULT (datetime('now'))
);

-- A coach's roster of trainees. Both sides point at users(id); the pair is unique.
CREATE TABLE coach_trainees (
    coach_id   INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    trainee_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    PRIMARY KEY (coach_id, trainee_id)
);
CREATE INDEX idx_coach_trainees_trainee ON coach_trainees(trainee_id);

-- The exercise library. Each exercise is owned by the coach who created it.
-- video_url is the library-default tutorial link (YouTube / Instagram).
CREATE TABLE exercises (
    id             INTEGER PRIMARY KEY,
    coach_id       INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    name           TEXT NOT NULL,
    category       TEXT,
    primary_muscle TEXT,
    description    TEXT,
    video_url      TEXT,
    created_at     TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE INDEX idx_exercises_coach ON exercises(coach_id);

CREATE TABLE workout_plans (
    id         INTEGER PRIMARY KEY,
    coach_id   INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    trainee_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    name       TEXT NOT NULL,
    notes      TEXT,
    is_active  INTEGER NOT NULL DEFAULT 0 CHECK (is_active IN (0, 1)),
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE INDEX idx_workout_plans_coach ON workout_plans(coach_id);
CREATE INDEX idx_workout_plans_trainee ON workout_plans(trainee_id);

-- One row per exercise slot inside a plan. video_url here, when set, overrides
-- the exercise's library link for this specific assignment.
CREATE TABLE plan_items (
    id            INTEGER PRIMARY KEY,
    plan_id       INTEGER NOT NULL REFERENCES workout_plans(id) ON DELETE CASCADE,
    exercise_id   INTEGER NOT NULL REFERENCES exercises(id) ON DELETE RESTRICT,
    order_index   INTEGER NOT NULL DEFAULT 0,
    day_label     TEXT,
    target_sets   INTEGER,
    target_reps   INTEGER,
    target_weight REAL,
    rest_seconds  INTEGER,
    notes         TEXT,
    video_url     TEXT
);
CREATE INDEX idx_plan_items_plan ON plan_items(plan_id);
CREATE INDEX idx_plan_items_exercise ON plan_items(exercise_id);

-- A workout a trainee actually performed. plan_id is nullable so a trainee can
-- log an ad-hoc session; ON DELETE SET NULL keeps the history if the plan goes.
CREATE TABLE workout_sessions (
    id           INTEGER PRIMARY KEY,
    trainee_id   INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    plan_id      INTEGER REFERENCES workout_plans(id) ON DELETE SET NULL,
    performed_at TEXT NOT NULL DEFAULT (datetime('now')),
    status       TEXT NOT NULL DEFAULT 'completed'
                 CHECK (status IN ('planned', 'in_progress', 'completed')),
    notes        TEXT
);
CREATE INDEX idx_workout_sessions_trainee ON workout_sessions(trainee_id);
CREATE INDEX idx_workout_sessions_plan ON workout_sessions(plan_id);

-- One row per set the trainee logged within a session. plan_item_id links the
-- set back to what was prescribed (nullable for ad-hoc sets).
CREATE TABLE session_sets (
    id           INTEGER PRIMARY KEY,
    session_id   INTEGER NOT NULL REFERENCES workout_sessions(id) ON DELETE CASCADE,
    exercise_id  INTEGER NOT NULL REFERENCES exercises(id) ON DELETE RESTRICT,
    plan_item_id INTEGER REFERENCES plan_items(id) ON DELETE SET NULL,
    set_number   INTEGER NOT NULL,
    reps         INTEGER,
    weight       REAL,
    rpe          REAL,
    completed    INTEGER NOT NULL DEFAULT 1 CHECK (completed IN (0, 1))
);
CREATE INDEX idx_session_sets_session ON session_sets(session_id);
CREATE INDEX idx_session_sets_exercise ON session_sets(exercise_id);
CREATE INDEX idx_session_sets_plan_item ON session_sets(plan_item_id);
