#!/usr/bin/env bash
# Manual end-to-end check of the Step 3 auth flow. Not part of the test suite.
set -u
PORT=18080
DB=/tmp/fp_smoke.db
LOG=/tmp/fp_smoke.log
BASE="http://127.0.0.1:${PORT}"

rm -f "$DB"
FITPLAN_DB_PATH="$DB" FITPLAN_PORT="$PORT" FITPLAN_JWT_SECRET=smoke-secret \
  FITPLAN_MIGRATIONS_DIR=src/db/migrations ./build/dev/fitplan >"$LOG" 2>&1 &
SRV=$!
trap 'kill $SRV 2>/dev/null' EXIT

for _ in $(seq 1 50); do
  curl -s -o /dev/null "${BASE}/api/health" && break
  sleep 0.1
done

code() { curl -s -o /dev/null -w '%{http_code}' "$@"; }
# The verification code the server just "sent" to <email> (no Brevo key -> the
# LogEmailSender writes the email body to the server log at warn level).
code_for() { grep -A8 "to=<$1>" "$LOG" | grep -oE '^ +[0-9]{6}$' | tail -1 | tr -d ' '; }

echo "== health =="
curl -s "${BASE}/api/health"; echo

echo "== register coach -> expect 201, verification_required, no token =="
curl -s -o /tmp/fp_reg.json -w 'status %{http_code}\n' -X POST "${BASE}/api/auth/register" \
  -H 'Content-Type: application/json' \
  -d '{"email":"coach@fp.com","password":"password123","role":"coach","display_name":"Coach One"}'
cat /tmp/fp_reg.json; echo

echo "== register duplicate email -> expect 409 =="
code -X POST "${BASE}/api/auth/register" -H 'Content-Type: application/json' \
  -d '{"email":"coach@fp.com","password":"password123","role":"coach","display_name":"Dup"}'; echo

echo "== register short password -> expect 400 =="
code -X POST "${BASE}/api/auth/register" -H 'Content-Type: application/json' \
  -d '{"email":"x@fp.com","password":"short","role":"coach","display_name":"X"}'; echo

echo "== register malformed JSON -> expect 400 =="
code -X POST "${BASE}/api/auth/register" -H 'Content-Type: application/json' -d 'not json'; echo

echo "== login before verifying -> expect 403 =="
code -X POST "${BASE}/api/auth/login" -H 'Content-Type: application/json' \
  -d '{"email":"coach@fp.com","password":"password123"}'; echo

echo "== verify-email wrong code -> expect 400 =="
code -X POST "${BASE}/api/auth/verify-email" -H 'Content-Type: application/json' \
  -d '{"email":"coach@fp.com","code":"000000"}'; echo

echo "== verify-email unknown address -> expect 404 =="
code -X POST "${BASE}/api/auth/verify-email" -H 'Content-Type: application/json' \
  -d '{"email":"nobody@fp.com","code":"123456"}'; echo

echo "== resend-verification -> expect 202 (always) =="
code -X POST "${BASE}/api/auth/resend-verification" -H 'Content-Type: application/json' \
  -d '{"email":"nobody@fp.com"}'; echo

echo "== verify-email correct code -> expect 200, returns a token =="
curl -s -o /tmp/fp_verify.json -w 'status %{http_code}\n' -X POST "${BASE}/api/auth/verify-email" \
  -H 'Content-Type: application/json' \
  -d "{\"email\":\"coach@fp.com\",\"code\":\"$(code_for coach@fp.com)\"}"
cat /tmp/fp_verify.json; echo

echo "== login correct (now verified) -> expect 200 =="
curl -s -o /tmp/fp_login.json -w 'status %{http_code}\n' -X POST "${BASE}/api/auth/login" \
  -H 'Content-Type: application/json' \
  -d '{"email":"coach@fp.com","password":"password123"}'
TOKEN=$(sed -n 's/.*"access_token":"\([^"]*\)".*/\1/p' /tmp/fp_login.json)
echo "token length: ${#TOKEN}"

echo "== login wrong password -> expect 401 =="
code -X POST "${BASE}/api/auth/login" -H 'Content-Type: application/json' \
  -d '{"email":"coach@fp.com","password":"nope"}'; echo

echo "== /me with token -> expect 200 =="
curl -s -w '\nstatus %{http_code}\n' "${BASE}/api/auth/me" -H "Authorization: Bearer ${TOKEN}"

echo "== /me without token -> expect 401 =="
code "${BASE}/api/auth/me"; echo

echo "== /me with garbage token -> expect 401 =="
code "${BASE}/api/auth/me" -H 'Authorization: Bearer not.a.jwt'; echo

echo "== request log =="
grep -E ' -> [0-9]' "$LOG" | tail -15
