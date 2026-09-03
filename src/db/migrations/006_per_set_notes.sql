-- 006_per_set_notes.sql
-- Move the free-text note off the session and onto each set: the trainee now
-- records "how did this set feel" per set instead of one note for the whole
-- workout. The migration runner wraps this file in a transaction, so both
-- ALTERs land together or not at all.

ALTER TABLE session_sets ADD COLUMN notes TEXT;

ALTER TABLE workout_sessions DROP COLUMN notes;