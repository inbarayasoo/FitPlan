-- 004_google_identity.sql
-- fitplan:no-transaction
--
-- Adds a second identity provider (Google / OIDC) to the users table:
--   * password_hash becomes NULLABLE - a Google-only account has no password.
--   * google_sub TEXT UNIQUE - Google's stable per-user id (the "sub" claim);
--     NULL for accounts created with email + password.
--
-- SQLite cannot drop a NOT NULL constraint in place, so this is the documented
-- "table rebuild": create the new shape, copy the rows, swap the names. That
-- procedure only preserves the foreign keys pointing at users(id) if foreign-key
-- enforcement is OFF while it runs - otherwise DROP TABLE users would cascade
-- into exercises / plans / sessions and delete everything. PRAGMA foreign_keys
-- is a no-op inside a transaction, so this file is marked "fitplan:no-transaction"
-- (the migration runner then does NOT wrap it) and manages its own BEGIN/COMMIT.

PRAGMA foreign_keys = OFF;

BEGIN;

DROP TABLE IF EXISTS users_new;

CREATE TABLE users_new (
    id            INTEGER PRIMARY KEY,
    email         TEXT NOT NULL UNIQUE,
    password_hash TEXT,                                  -- nullable: Google-only users have none
    role          TEXT NOT NULL CHECK (role IN ('trainee', 'coach')),
    display_name  TEXT NOT NULL,
    created_at    TEXT NOT NULL DEFAULT (datetime('now')),
    auth_provider TEXT NOT NULL DEFAULT 'local'
                  CHECK (auth_provider IN ('local', 'google')),
    google_sub    TEXT UNIQUE                            -- Google "sub" claim; NULL for local users
);

INSERT INTO users_new (id, email, password_hash, role, display_name, created_at, auth_provider)
SELECT id, email, password_hash, role, display_name, created_at, auth_provider
FROM users;

DROP TABLE users;

ALTER TABLE users_new RENAME TO users;

COMMIT;

PRAGMA foreign_keys = ON;
