-- 002_auth_provider.sql
-- Records which identity provider created each user account. Every row so far was
-- created with an email + password, so 'local' is both the column default and the
-- backfill for existing rows. A second provider (Google / OIDC) is planned; when
-- it lands it will store 'google' here, and its own migration will relax the
-- password_hash NOT NULL constraint that local accounts still rely on.

ALTER TABLE users
    ADD COLUMN auth_provider TEXT NOT NULL DEFAULT 'local'
    CHECK (auth_provider IN ('local', 'google'));
