-- seed.sql
-- Sample data so a fresh clone has something to show immediately: one coach, one
-- trainee (linked), a small exercise library with real tutorial links, one
-- active plan with items, and two logged sessions with sets.
--
-- Safe to run more than once: it clears the tables first. It never touches
-- schema_version, so the schema itself is left alone.
--
-- Login credentials for the seeded accounts (the password_hash values are real
-- Argon2id hashes of these passwords). Both are seeded email_verified = 1 so the
-- demo logins work without an email round-trip:
--   coach@fitplan.dev    / coach-demo-pass
--   trainee@fitplan.dev  / trainee-demo-pass

DELETE FROM session_sets;
DELETE FROM workout_sessions;
DELETE FROM plan_items;
DELETE FROM workout_plans;
DELETE FROM exercises;
DELETE FROM coach_trainees;
DELETE FROM users;

-- ----- accounts -----------------------------------------------------------
INSERT INTO users (id, email, password_hash, role, display_name, email_verified) VALUES
    (1, 'coach@fitplan.dev',   '$argon2id$v=19$m=65536,t=2,p=1$DobN91CmhudmltQh+IQGbw$MV7QM7fX/PPgEU5pWmEqjcXXavvEYiN8tMWnMTeVfaI', 'coach',   'Dana Coach',   1),
    (2, 'trainee@fitplan.dev', '$argon2id$v=19$m=65536,t=2,p=1$jvSKpMnHMoO7hUgoFArBWA$ljmqnKoM9rYXTnpFchOpyP4PXJ+2TQAqwA95lQbxdTQ', 'trainee', 'Ron Trainee', 1);

INSERT INTO coach_trainees (coach_id, trainee_id) VALUES (1, 2);

-- ----- exercise library (owned by the coach) ----------------------------
INSERT INTO exercises (id, coach_id, name, category, primary_muscle, description, video_url) VALUES
    (1, 1, 'Back Squat',        'lower', 'quadriceps', 'Barbell high-bar back squat to depth.',        'https://www.youtube.com/watch?v=ultWZbUMPL8'),
    (2, 1, 'Bench Press',       'upper', 'chest',      'Flat barbell bench press, full range.',        'https://www.youtube.com/watch?v=rT7DgCr-3pg'),
    (3, 1, 'Deadlift',          'lower', 'hamstrings', 'Conventional barbell deadlift from the floor.','https://www.youtube.com/watch?v=op9kVnSso6Q'),
    (4, 1, 'Overhead Press',    'upper', 'shoulders',  'Standing barbell strict press.',               'https://www.youtube.com/watch?v=2yjwXTZQDDI'),
    (5, 1, 'Barbell Row',       'upper', 'back',       'Bent-over barbell row, torso ~45 degrees.',    'https://www.youtube.com/watch?v=9efgcAjQe7E'),
    (6, 1, 'Pull-up',           'upper', 'back',       'Dead-hang pull-up, chin over the bar.',        'https://www.youtube.com/watch?v=eGo4IYlbE5g'),
    (7, 1, 'Romanian Deadlift', 'lower', 'hamstrings', 'Hip-hinge RDL, slight knee bend.',            'https://www.youtube.com/watch?v=JCXUYuzwNrM'),
    (8, 1, 'Plank',             'core',  'abdominals', 'Front plank, neutral spine, timed hold.',      'https://www.youtube.com/watch?v=pSHjTRCQxIw');

-- ----- one active plan for the trainee ---------------------------------
INSERT INTO workout_plans (id, coach_id, trainee_id, name, notes, is_active) VALUES
    (1, 1, 2, 'Full Body A/B - Weeks 1-4', 'Three sessions per week, alternate day A and day B.', 1);

INSERT INTO plan_items
    (id, plan_id, exercise_id, order_index, day_label, target_sets, target_reps, target_weight, rest_seconds, notes) VALUES
    (1, 1, 1, 0, 'A', 3, 5,  80.0, 180, 'Add 2.5 kg when all sets hit 5 reps.'),
    (2, 1, 2, 1, 'A', 3, 5,  60.0, 180, NULL),
    (3, 1, 5, 2, 'A', 3, 8,  50.0, 120, NULL),
    (4, 1, 3, 0, 'B', 1, 5, 100.0, 240, 'One heavy top set.'),
    (5, 1, 4, 1, 'B', 3, 6,  35.0, 150, NULL),
    (6, 1, 6, 2, 'B', 3, 8,   0.0, 120, 'Bodyweight; add a band if needed.');

-- ----- two logged sessions (day A), two weeks apart ------------------
INSERT INTO workout_sessions (id, trainee_id, plan_id, performed_at, status) VALUES
    (1, 2, 1, datetime('now', '-14 days'), 'completed'),
    (2, 2, 1, datetime('now', '-7 days'),  'completed');

-- per-set notes: the trainee's "how did it feel" line, left mostly NULL
INSERT INTO session_sets
    (session_id, exercise_id, plan_item_id, set_number, reps, weight, rpe, completed, notes) VALUES
    (1, 1, 1, 1, 5, 80.0, 7.0, 1, 'Felt strong off the floor.'),
    (1, 1, 1, 2, 5, 80.0, 7.5, 1, NULL),
    (1, 1, 1, 3, 5, 80.0, 8.0, 1, NULL),
    (1, 2, 2, 1, 5, 60.0, 7.0, 1, NULL),
    (1, 2, 2, 2, 5, 60.0, 7.5, 1, NULL),
    (1, 2, 2, 3, 4, 60.0, 9.0, 1, 'Last rep grindy; stopped a rep short.'),
    (2, 1, 1, 1, 5, 82.5, 7.5, 1, NULL),
    (2, 1, 1, 2, 5, 82.5, 8.0, 1, NULL),
    (2, 1, 1, 3, 5, 82.5, 8.5, 1, NULL),
    (2, 2, 2, 1, 5, 62.5, 7.5, 1, NULL),
    (2, 2, 2, 2, 5, 62.5, 8.0, 1, NULL),
    (2, 2, 2, 3, 5, 62.5, 8.5, 1, NULL);
