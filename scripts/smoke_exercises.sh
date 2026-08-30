#!/usr/bin/env bash
# Manual end-to-end check of the Step 4 exercise-library API. Not part of the
# test suite. Starts a fresh server on a temp DB, then walks the coach CRUD flow
# plus the authz and video_url-validation edge cases.
set -u
PORT=18081
DB=/tmp/fp_smoke_ex.db
LOG=/tmp/fp_smoke_ex.log
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
token_of() { sed -n 's/.*"access_token":"\([^"]*\)".*/\1/p' "$1"; }

echo "== register coach A -> 201 =="
curl -s -o /tmp/fp_ex_a.json -w 'status %{http_code}\n' -X POST "${BASE}/api/auth/register" \
  -H 'Content-Type: application/json' \
  -d '{"email":"coachA@fp.com","password":"password123","role":"coach","display_name":"Coach A"}'
A=$(token_of /tmp/fp_ex_a.json)

echo "== register coach B -> 201 =="
curl -s -o /tmp/fp_ex_b.json -w 'status %{http_code}\n' -X POST "${BASE}/api/auth/register" \
  -H 'Content-Type: application/json' \
  -d '{"email":"coachB@fp.com","password":"password123","role":"coach","display_name":"Coach B"}'
B=$(token_of /tmp/fp_ex_b.json)

echo "== register trainee T -> 201 =="
curl -s -o /tmp/fp_ex_t.json -w 'status %{http_code}\n' -X POST "${BASE}/api/auth/register" \
  -H 'Content-Type: application/json' \
  -d '{"email":"traineeT@fp.com","password":"password123","role":"trainee","display_name":"Trainee T"}'
T=$(token_of /tmp/fp_ex_t.json)

auth_a=(-H "Authorization: Bearer ${A}")
auth_b=(-H "Authorization: Bearer ${B}")
auth_t=(-H "Authorization: Bearer ${T}")
json=(-H 'Content-Type: application/json')

echo
echo "== A creates an exercise (YouTube link) -> 201, body has video_embed_url =="
curl -s -o /tmp/fp_ex1.json -w 'status %{http_code}\n' -X POST "${BASE}/api/exercises" \
  "${auth_a[@]}" "${json[@]}" \
  -d '{"name":"Back Squat","category":"legs","primary_muscle":"quadriceps","video_url":"https://www.youtube.com/watch?v=dQw4w9WgXcQ"}'
cat /tmp/fp_ex1.json; echo
EX_ID=$(sed -n 's/.*"id":\([0-9]*\).*/\1/p' /tmp/fp_ex1.json)
echo "exercise id = ${EX_ID}"

echo
echo "== A creates a second exercise (no video) -> 201 =="
code -X POST "${BASE}/api/exercises" "${auth_a[@]}" "${json[@]}" \
  -d '{"name":"Deadlift"}'; echo

echo "== A lists exercises -> 200, two rows =="
curl -s "${BASE}/api/exercises" "${auth_a[@]}"; echo

echo "== B lists exercises -> 200, zero rows (isolation) =="
curl -s "${BASE}/api/exercises" "${auth_b[@]}"; echo

echo
echo "== A gets its exercise by id -> 200 =="
code "${BASE}/api/exercises/${EX_ID}" "${auth_a[@]}"; echo

echo "== B gets A's exercise by id -> 404 (not yours) =="
code "${BASE}/api/exercises/${EX_ID}" "${auth_b[@]}"; echo

echo "== A gets a missing id -> 404 =="
code "${BASE}/api/exercises/999999" "${auth_a[@]}"; echo

echo
echo "== A updates its exercise -> 200 =="
curl -s -w '\nstatus %{http_code}\n' -X PUT "${BASE}/api/exercises/${EX_ID}" \
  "${auth_a[@]}" "${json[@]}" \
  -d '{"name":"Back Squat (paused)","category":"legs","video_url":"https://youtu.be/dQw4w9WgXcQ"}'

echo "== B updates A's exercise -> 404 =="
code -X PUT "${BASE}/api/exercises/${EX_ID}" "${auth_b[@]}" "${json[@]}" \
  -d '{"name":"hijack"}'; echo

echo
echo "== validation: http (not https) -> 400 =="
code -X POST "${BASE}/api/exercises" "${auth_a[@]}" "${json[@]}" \
  -d '{"name":"x","video_url":"http://www.youtube.com/watch?v=abc"}'; echo

echo "== validation: javascript: URL -> 400 =="
code -X POST "${BASE}/api/exercises" "${auth_a[@]}" "${json[@]}" \
  -d '{"name":"x","video_url":"javascript:alert(1)"}'; echo

echo "== validation: disallowed host (vimeo) -> 400 =="
code -X POST "${BASE}/api/exercises" "${auth_a[@]}" "${json[@]}" \
  -d '{"name":"x","video_url":"https://vimeo.com/123"}'; echo

echo "== validation: spoofed host youtube.com@evil.com -> 400 =="
code -X POST "${BASE}/api/exercises" "${auth_a[@]}" "${json[@]}" \
  -d '{"name":"x","video_url":"https://youtube.com@evil.com/watch?v=abc"}'; echo

echo "== validation: blank name -> 400 =="
code -X POST "${BASE}/api/exercises" "${auth_a[@]}" "${json[@]}" \
  -d '{"name":"   "}'; echo

echo "== validation: Instagram link accepted -> 201 =="
code -X POST "${BASE}/api/exercises" "${auth_a[@]}" "${json[@]}" \
  -d '{"name":"Mobility drill","video_url":"https://www.instagram.com/reel/Cabcdef1234/"}'; echo

echo
echo "== authz: trainee hits a coach route -> 403 =="
code "${BASE}/api/exercises" "${auth_t[@]}"; echo

echo "== authz: no token -> 401 =="
code "${BASE}/api/exercises"; echo

echo
echo "== A deletes its exercise -> 204 =="
code -X DELETE "${BASE}/api/exercises/${EX_ID}" "${auth_a[@]}"; echo

echo "== A deletes it again -> 404 =="
code -X DELETE "${BASE}/api/exercises/${EX_ID}" "${auth_a[@]}"; echo

echo
echo "== request log =="
grep -E ' -> [0-9]' "$LOG" | tail -30
