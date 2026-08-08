/*
 * PointerThere - Next generation Geometry Dash Demon List
 * Copyright (C) 2026 PointerThere — GPLv3
 */

CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
CREATE EXTENSION IF NOT EXISTS "pg_trgm";
CREATE EXTENSION IF NOT EXISTS "citext";

CREATE TABLE IF NOT EXISTS users (
  id                  BIGSERIAL PRIMARY KEY,
  username            CITEXT NOT NULL UNIQUE,
  email               CITEXT NOT NULL UNIQUE,
  password_hash       TEXT,
  avatar_url          TEXT,
  bio                 TEXT DEFAULT '',
  country             TEXT DEFAULT '',
  continent           TEXT DEFAULT '',
  is_public           BOOLEAN NOT NULL DEFAULT TRUE,
  points              NUMERIC(12,4) NOT NULL DEFAULT 0,
  email_verified      BOOLEAN NOT NULL DEFAULT FALSE,
  email_verify_token  TEXT,
  reset_token         TEXT,
  reset_token_expires TIMESTAMPTZ,
  two_factor_enabled  BOOLEAN NOT NULL DEFAULT FALSE,
  two_factor_secret   TEXT,
  two_factor_code_expires TIMESTAMPTZ,
  discord_id          TEXT UNIQUE,
  discord_username    TEXT,
  google_id           TEXT UNIQUE,
  is_banned           BOOLEAN NOT NULL DEFAULT FALSE,
  ban_reason          TEXT,
  ban_expires_at      TIMESTAMPTZ,
  timeout_until       TIMESTAMPTZ,
  created_at          TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  updated_at          TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
CREATE INDEX IF NOT EXISTS idx_users_email      ON users (email);
CREATE INDEX IF NOT EXISTS idx_users_discord_id ON users (discord_id);
CREATE INDEX IF NOT EXISTS idx_users_google_id  ON users (google_id);
CREATE INDEX IF NOT EXISTS idx_users_points     ON users (points DESC);
CREATE INDEX IF NOT EXISTS idx_users_username_trgm ON users USING GIN (username gin_trgm_ops);

-- Safe to run on an existing Railway/Neon database when deploying email 2FA.
ALTER TABLE users ADD COLUMN IF NOT EXISTS two_factor_code_expires TIMESTAMPTZ;

CREATE TABLE IF NOT EXISTS demon_levels (
  id              BIGSERIAL PRIMARY KEY,
  rank            INTEGER NOT NULL UNIQUE,
  name            CITEXT NOT NULL UNIQUE,
  points          NUMERIC(12,4) NOT NULL DEFAULT 0,
  verified_by     TEXT NOT NULL,
  creators        TEXT[] NOT NULL DEFAULT '{}',
  video_url       TEXT NOT NULL,
  thumbnail_url   TEXT,
  difficulty_tier TEXT NOT NULL CHECK (difficulty_tier IN ('extreme', 'insane')),
  is_featured     BOOLEAN NOT NULL DEFAULT FALSE,
  created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
CREATE INDEX IF NOT EXISTS idx_demon_levels_rank       ON demon_levels (rank);
CREATE INDEX IF NOT EXISTS idx_demon_levels_name_trgm  ON demon_levels USING GIN (name gin_trgm_ops);
CREATE INDEX IF NOT EXISTS idx_demon_levels_featured   ON demon_levels (is_featured) WHERE is_featured = TRUE;

CREATE TABLE IF NOT EXISTS list_movements (
  id          BIGSERIAL PRIMARY KEY,
  level_id    BIGINT NOT NULL REFERENCES demon_levels(id) ON DELETE CASCADE,
  old_rank    INTEGER,
  new_rank    INTEGER NOT NULL,
  created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
CREATE INDEX IF NOT EXISTS idx_list_movements_created ON list_movements (created_at DESC);

CREATE TABLE IF NOT EXISTS records (
  id              BIGSERIAL PRIMARY KEY,
  level_id        BIGINT NOT NULL REFERENCES demon_levels(id) ON DELETE CASCADE,
  user_id         BIGINT REFERENCES users(id) ON DELETE SET NULL,
  player_name     TEXT NOT NULL,
  progress        SMALLINT NOT NULL CHECK (progress BETWEEN 1 AND 100),
  video_url       TEXT NOT NULL,
  notes           TEXT DEFAULT '',
  discord_tag     TEXT,
  status          TEXT NOT NULL DEFAULT 'pending'
                    CHECK (status IN ('pending', 'accepted', 'rejected', 'flagged')),
  reviewed_at     TIMESTAMPTZ,
  submitted_at    TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
CREATE INDEX IF NOT EXISTS idx_records_status       ON records (status);
CREATE INDEX IF NOT EXISTS idx_records_level_id     ON records (level_id);
CREATE INDEX IF NOT EXISTS idx_records_user_id      ON records (user_id);
CREATE INDEX IF NOT EXISTS idx_records_submitted_at ON records (submitted_at ASC);

CREATE TABLE IF NOT EXISTS appeals (
  id          BIGSERIAL PRIMARY KEY,
  user_id     BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  reason      TEXT NOT NULL,
  status      TEXT NOT NULL DEFAULT 'pending'
                CHECK (status IN ('pending', 'resolved_lifted', 'resolved_banned')),
  reviewed_at TIMESTAMPTZ,
  created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
CREATE INDEX IF NOT EXISTS idx_appeals_status     ON appeals (status);
CREATE INDEX IF NOT EXISTS idx_appeals_created_at ON appeals (created_at ASC);

CREATE TABLE IF NOT EXISTS api_keys (
  id              BIGSERIAL PRIMARY KEY,
  user_id         BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  name            TEXT NOT NULL,
  key_hash        TEXT NOT NULL UNIQUE,
  key_prefix      TEXT NOT NULL,
  monthly_usage   INTEGER NOT NULL DEFAULT 0,
  last_used_at    TIMESTAMPTZ,
  created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
CREATE INDEX IF NOT EXISTS idx_api_keys_user_id  ON api_keys (user_id);
CREATE INDEX IF NOT EXISTS idx_api_keys_key_hash ON api_keys (key_hash);

CREATE TABLE IF NOT EXISTS site_settings (
  id                  INTEGER PRIMARY KEY DEFAULT 1 CHECK (id = 1),
  discord_url         TEXT DEFAULT '',
  twitter_url         TEXT DEFAULT '',
  youtube_url         TEXT DEFAULT '',
  twitch_url          TEXT DEFAULT '',
  github_url          TEXT DEFAULT '',
  patreon_url         TEXT DEFAULT '',
  db_cost             NUMERIC(8,2) NOT NULL DEFAULT 0,
  deploy_cost         NUMERIC(8,2) NOT NULL DEFAULT 0,
  bot_cost            NUMERIC(8,2) NOT NULL DEFAULT 0,
  featured_level_id   BIGINT REFERENCES demon_levels(id) ON DELETE SET NULL,
  updated_at          TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

INSERT INTO site_settings (id) VALUES (1) ON CONFLICT (id) DO NOTHING;

CREATE OR REPLACE FUNCTION set_updated_at()
RETURNS TRIGGER AS $$
BEGIN
  NEW.updated_at = NOW();
  RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trigger_users_updated_at        BEFORE UPDATE ON users          FOR EACH ROW EXECUTE FUNCTION set_updated_at();
CREATE TRIGGER trigger_demon_levels_updated_at BEFORE UPDATE ON demon_levels   FOR EACH ROW EXECUTE FUNCTION set_updated_at();
CREATE TRIGGER trigger_records_updated_at      BEFORE UPDATE ON records        FOR EACH ROW EXECUTE FUNCTION set_updated_at();
CREATE TRIGGER trigger_site_settings_updated_at BEFORE UPDATE ON site_settings FOR EACH ROW EXECUTE FUNCTION set_updated_at();
