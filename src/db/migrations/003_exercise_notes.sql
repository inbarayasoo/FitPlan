-- 003_exercise_notes.sql
-- A trainee's own free-text notes for an exercise: the coaching cues they want
-- to keep next to a movement ("brace before the pull", "elbows in", ...). One
-- note per (trainee, exercise); a PUT upserts it. Kept separate from
-- plan_items.notes, which is the coach's per-assignment note and is rewritten
-- every time the coach edits the plan.

CREATE TABLE exercise_notes (
    id          INTEGER PRIMARY KEY,
    trainee_id  INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    exercise_id INTEGER NOT NULL REFERENCES exercises(id) ON DELETE CASCADE,
    body        TEXT NOT NULL,
    updated_at  TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE (trainee_id, exercise_id)
);
CREATE INDEX idx_exercise_notes_trainee ON exercise_notes(trainee_id);
