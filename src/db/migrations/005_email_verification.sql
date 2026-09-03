-- 005_email_verification.sql
-- Local (email + password) sign-ups must now prove they own the address before
-- they can log in. This adds:
--   * users.email_verified INTEGER 0/1 - 1 once the address is confirmed.
--     Existing rows are backfilled to 1: they predate this requirement and are
--     grandfathered in. Google accounts are created with 1 by the app (Google
--     has already verified the address).
--   * email_verification_tokens - at most one pending 6-digit code per user
--     (user_id is UNIQUE). Only the SHA-256 hash of the code is stored; the row
--     carries an expiry and an attempt counter so a short numeric code cannot be
--     brute-forced. issued_at is written by the service (from its injected clock),
--     not defaulted here, so the resend cooldown is testable.
-- A plain additive migration: it runs inside the migration runner's transaction.

ALTER TABLE users
    ADD COLUMN email_verified INTEGER NOT NULL DEFAULT 0
    CHECK (email_verified IN (0, 1));

UPDATE users SET email_verified = 1;

-- user_id UNIQUE already provides the index the foreign key needs.
CREATE TABLE email_verification_tokens (
    id         INTEGER PRIMARY KEY,
    user_id    INTEGER NOT NULL UNIQUE REFERENCES users(id) ON DELETE CASCADE,
    code_hash  TEXT NOT NULL,
    expires_at TEXT NOT NULL,
    attempts   INTEGER NOT NULL DEFAULT 0,
    issued_at  TEXT NOT NULL
);
