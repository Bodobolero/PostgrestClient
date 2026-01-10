# Self-hosted PostgreSQL and Postgrest App server

This file contains an example setup that shows how to use this PostgrestClient library with a self-hosted upstream/vanilla Postgres.

The example was tested on Ubuntu 24.04 LTS on x86-64 on a server in home network with IP address 192.168.178.90.

Adjust all settings to your own self-hosted environment (e.g. use your own server's IP address).

## Security Alert

PostgREST itself serves plain HTTP. The standard, secure way to add HTTPS is to put it behind a TLS-terminating reverse proxy (nginx or Caddy) and bind PostgREST to 127.0.0.1 only.

The setup below doesn't use a reverse proxy and binds PostGrest to 0.0.0.0 making the insecure HTTP port available within the network.
This is acceptable for a home Wifi but shouldn't be used for production.

## Install self-hosted postgres on  homeserver natively (Deban apt package)

```bash
# avoid update system hangs on ipv6 system
sudo sh -c 'echo "Acquire::ForceIPv4 \"true\";" > /etc/apt/apt.conf.d/99force-ipv4'
# update system
sudo apt update
# update prereqs
sudo apt install -y postgresql-common ca-certificates curl
# enable postgres repos
sudo /usr/share/postgresql-common/pgdg/apt.postgresql.org.sh
# install postgres
sudo apt install -y postgresql-18 postgresql-client-18 postgresql-contrib-18
# expose TPC port 5432
cd /etc/postgresql/18/main
sudo vi postgresql.conf
# listen_addresses = '*' 
sudo vi pg_hba.conf
# host    all    all    0.0.0.0/0    scram-sha-256

#  enable service and start it
sudo systemctl enable postgresql
sudo systemctl start postgresql
# change default password
sudo -u postgres psql
# ALTER USER postgres WITH PASSWORD 'mysecretadminpassword';

sudo systemctl restart postgresql
```


## Postgrest installation and configuration

### Installation

```bash
# pick a version from the releases page; example uses "v14.2"
VER=v14.2

cd /tmp
curl -L -o postgrest.tar.xz \
  "https://github.com/PostgREST/postgrest/releases/download/${VER}/postgrest-${VER}-linux-static-x86-64.tar.xz"

tar -xf postgrest.tar.xz
sudo install -m 0755 postgrest /usr/local/bin/postgrest

postgrest --version
sudo mkdir -p /etc/postgrest
sudo chmod 0755 /etc/postgrest
# generate jwt secret (see below)
openssl rand -base64 32
```


### postgrest configuration

```sql
create database sensorvalues;
\c sensorvalues;

-- authenticator (LOGIN) → used in db-uri to connect postgrest app server to postgres
-- anonymous (NOLOGIN) → used in db-anon-role
-- authenticated (NOLOGIN) → used in JWT role claim and in RLS policies (TO authenticated)
CREATE ROLE authenticator NOINHERIT LOGIN PASSWORD 'secret';
create role authenticated nologin;
create role anonymous nologin;

GRANT anonymous     TO authenticator;
GRANT authenticated TO authenticator;

grant usage on schema public to authenticated, anonymous;
create schema if not exists auth;
grant usage on schema auth   to authenticated, anonymous;

alter system set app.jwt_secret =
  '>32charsjwtsecret...';
```
config

sudo vi /etc/postgrest/postgrest.conf

```conf
db-uri = "postgres://authenticator:secret@127.0.0.1:5432/sensorvalues"

db-schemas = "public,auth"
db-anon-role = "anonymous"

jwt-secret = ">32charsjwtsecret..."

server-host = "0.0.0.0"
server-port = 3000
```

sudo vi /etc/systemd/system/postgrest.service

```conf
[Unit]
Description=PostgREST
After=network.target postgresql.service

[Service]
User=www-data
Group=www-data
ExecStart=/usr/local/bin/postgrest /etc/postgrest/postgrest.conf
Restart=on-failure
RestartSec=2

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now postgrest
sudo systemctl status postgrest --no-pager
```

### Tables/functions for auth

```sql
create extension if not exists pgcrypto;
create extension if not exists citext;

-- used for JWT creation and verification
alter system set app.jwt_secret =
  'CHANGE_ME_TO_A_32+_CHAR_SECRET________________________________';
select pg_reload_conf();


create schema if not exists auth;

create table if not exists auth.users (
  id uuid primary key default gen_random_uuid(),
  email citext unique not null,
  pass_hash text not null,
  created_at timestamptz not null default now()
);

revoke all on auth.users from public;

-- create key using e.g.
-- openssl rand -base64 32
-- signup key is to avoid denial of service - only API callers with this key
-- can register new users
ALTER SYSTEM SET app.signup_key = 'CHANGE_ME_LONG_RANDOM';
SELECT pg_reload_conf();

-- function to register new users, usage see curl script 01_auth_signup_with_email.sh
CREATE OR REPLACE FUNCTION auth.create_user(email text, password text, api_key text)
RETURNS uuid
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = auth, public
AS $$
DECLARE
  expected text;
  new_id uuid;
BEGIN
  expected := current_setting('app.signup_key', true);

  IF expected IS NULL OR expected = '' THEN
    RAISE EXCEPTION 'signup disabled';
  END IF;

  IF api_key IS DISTINCT FROM expected THEN
    RAISE EXCEPTION 'invalid signup key' USING errcode = '28000';
  END IF;

  INSERT INTO auth.users(email, pass_hash)
  VALUES (email::citext, crypt(password, gen_salt('bf', 12)))
  RETURNING id INTO new_id;

  RETURN new_id;
END;
$$;

REVOKE ALL ON FUNCTION auth.create_user(text,text,text) FROM PUBLIC;
GRANT EXECUTE ON FUNCTION auth.create_user(text,text,text) TO anonymous;  -- if you want pre-login signup

create or replace function auth.base64url_encode(input text)
returns text language sql immutable as $$
  select translate(
    replace(replace(encode(convert_to(input,'utf8'),'base64'), E'\n',''), '=', ''),
    '+/','-_'
  );
$$;

create or replace function auth.base64url_encode_bytes(input bytea)
returns text language sql immutable as $$
  select translate(
    replace(replace(encode(input,'base64'), E'\n',''), '=', ''),
    '+/','-_'
  );
$$;

create or replace function auth.jwt_sign_hs256(payload jsonb, secret text)
returns text
language plpgsql
immutable
as $$
declare
  header_b64 text := auth.base64url_encode('{"alg":"HS256","typ":"JWT"}');
  payload_b64 text := auth.base64url_encode(payload::text);
  signing_input text := header_b64 || '.' || payload_b64;
  sig bytea := hmac(signing_input, secret, 'sha256');
begin
  return signing_input || '.' || auth.base64url_encode_bytes(sig);
end;
$$;

-- login RPC, login a user created before with email/password
-- note that this simple auth implementation does not verify
-- the email like a third party auth service
-- because I wanted to avoid the complexity of setting up an 
-- SMTP mail server and creating/verifying OTP codes for email verification
create or replace function auth.login(email text, password text)
returns jsonb
language plpgsql
security definer
set search_path = auth, public
as $$
declare
  u auth.users;
  secret text;
  now_epoch int;
  exp_epoch int;
  claims jsonb;
begin
  select * into u
  from auth.users
  where users.email = login.email::citext;

  if not found or u.pass_hash <> crypt(login.password, u.pass_hash) then
    raise exception 'invalid email or password' using errcode = '28000';
  end if;

  secret := current_setting('app.jwt_secret', true);
  if secret is null or length(secret) < 32 then
    raise exception 'jwt secret not configured';
  end if;

  now_epoch := extract(epoch from now())::int;
  exp_epoch := now_epoch + 3600;

  claims := jsonb_build_object(
    'role', 'authenticated',
    'sub',  u.id::text,
    'email', u.email::text,
    'iat', now_epoch,
    'exp', exp_epoch
  );

  return jsonb_build_object(
    'token', auth.jwt_sign_hs256(claims, secret)
  );
end;
$$;

grant execute on function auth.login(text,text) to anonymous;
revoke execute on function auth.login(text,text) from authenticated;
```

### Tables for sensor values and row level secrity (RLS) policies

```sql
-- =========================
-- 1) Helper: auth.user_id() -> user_id from JWT
-- (works for PostgREST requests because it sets request.jwt.claim.* GUCs)
-- =========================
CREATE OR REPLACE FUNCTION auth.user_id()
RETURNS text
LANGUAGE sql
STABLE
AS $$
  SELECT COALESCE(
    current_setting('request.jwt.claim.sub', true),
    (current_setting('request.jwt.claims', true)::json ->> 'sub')
  );
$$;

REVOKE ALL ON FUNCTION auth.user_id() FROM public;
GRANT EXECUTE ON FUNCTION auth.user_id() TO authenticated, anonymous;

-- =========================
-- 2) Tables 
-- =========================
CREATE TABLE IF NOT EXISTS public.actorvalues (
  sent_time          timestamp DEFAULT now(),
  user_id            text DEFAULT (auth.user_id()) NOT NULL,
  acknowledge_time   timestamp,
  actor_name         text,
  actor_value        text NOT NULL,
  CONSTRAINT actorvalues_pkey PRIMARY KEY (actor_name, sent_time)
);

CREATE TABLE IF NOT EXISTS public.sensorvalues (
  measure_time  timestamp DEFAULT now(),
  user_id       text DEFAULT (auth.user_id()) NOT NULL,
  sensor_name   text,
  sensor_value  double precision,
  CONSTRAINT sensorvalues_pkey PRIMARY KEY (sensor_name, measure_time)
);

-- Optional but usually helpful for per-user queries
CREATE INDEX IF NOT EXISTS actorvalues_user_time_idx
  ON public.actorvalues (user_id, sent_time DESC);

CREATE INDEX IF NOT EXISTS sensorvalues_user_time_idx
  ON public.sensorvalues (user_id, measure_time DESC);

-- =========================
-- 3) Grants
-- =========================

-- Authenticated users can CRUD, but RLS restricts rows
GRANT SELECT, INSERT, UPDATE, DELETE ON public.actorvalues  TO authenticated;
GRANT SELECT, INSERT, UPDATE, DELETE ON public.sensorvalues TO authenticated;

-- If you want anonymous access, uncomment (RLS rules for anonymous below control what they see)
-- GRANT SELECT ON public.actorvalues  TO anonymous;
-- GRANT SELECT ON public.sensorvalues TO anonymous;

--- =========================
-- 4) RLS policies (use auth.user_id() helper)
-- =========================
ALTER TABLE public.actorvalues  ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.sensorvalues ENABLE ROW LEVEL SECURITY;

DROP POLICY IF EXISTS actorvalues_own ON public.actorvalues;
CREATE POLICY actorvalues_own ON public.actorvalues
FOR ALL
TO authenticated
USING (user_id = auth.user_id())
WITH CHECK (user_id = auth.user_id());

DROP POLICY IF EXISTS sensorvalues_own ON public.sensorvalues;
CREATE POLICY sensorvalues_own ON public.sensorvalues
FOR ALL
TO authenticated
USING (user_id = auth.user_id())
WITH CHECK (user_id = auth.user_id());

-- =========================
-- 5) Anti-spoofing: force user_id from JWT on INSERT/UPDATE
-- This prevents a client from writing rows for other users
-- =========================
CCREATE OR REPLACE FUNCTION public.set_user_id_from_jwt()
RETURNS trigger
LANGUAGE plpgsql
AS $$
BEGIN
  NEW.user_id := auth.user_id();  -- single source of truth
  RETURN NEW;
END;
$$;

DROP TRIGGER IF EXISTS trg_actorvalues_set_user_id ON public.actorvalues;
CREATE TRIGGER trg_actorvalues_set_user_id
BEFORE INSERT OR UPDATE ON public.actorvalues
FOR EACH ROW
EXECUTE FUNCTION public.set_user_id_from_jwt();

DROP TRIGGER IF EXISTS trg_sensorvalues_set_user_id ON public.sensorvalues;
CREATE TRIGGER trg_sensorvalues_set_user_id
BEFORE INSERT OR UPDATE ON public.sensorvalues
FOR EACH ROW
EXECUTE FUNCTION public.set_user_id_from_jwt();

NOTIFY pgrst, 'reload schema'
```

### Sign up a new user

```bash
curl -s -X POST http://127.0.0.1:3000/rpc/create_user \
  -H "Content-Type: application/json" \
   -H "Content-Profile: auth" \
  -d '{"email":"alice@example.com","password":"S3curePassw0rd!","api_key":"CHANGE_ME_LONG_RANDOM"}'
```

### login user to receive a JWT

```bash
curl -s -X POST http://127.0.0.1:3000/rpc/login   \
 -H "Content-Type: application/json"  \
 -H "Content-Profile: auth" \
 -d '{"email":"alice@example.com","password":"S3curePassw0rd!"}'
```

### Other examples

See other examples in this directory.


