#!/usr/bin/env bash
# Manual end-to-end check of the Step 4 roster + plan API. Not part of the test
# suite. Walks: attach trainees -> build a plan with items -> read/update ->
# assign -> plus the authz and validation edge cases.
set -u
PORT=18082
DB=/tmp/fp_smoke_plans.db
LOG=/tmp/fp_smoke_plans.log
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
# First "id":N in the file. grep (not sed) so a nested items[].id cannot be
# picked up by a greedy match.
id_of() { grep -o '"id":[0-9]\+' "$1" | head -1 | grep -o '[0-9]\+'; }

reg() { # email role -> writes /tmp/fp_$1.json, echoes token
  curl -s -o "/tmp/fp_$3.json" -X POST "${BASE}/api/auth/register" \
    -H 'Content-Type: application/json' \
    -d "{\"email\":\"$1\",\"password\":\"password123\",\"role\":\"$2\",\"display_name\":\"$3\"}"
  token_of "/tmp/fp_$3.json"
}

A=$(reg coachA@fp.com coach A)
B=$(reg coachB@fp.com coach B)
T1=$(reg t1@fp.com trainee T1)
T2=$(reg t2@fp.com trainee T2)
reg t3@fp.com trainee T3 >/dev/null

auth_a=(-H "Authorization: Bearer ${A}")
auth_b=(-H "Authorization: Bearer ${B}")
auth_t1=(-H "Authorization: Bearer ${T1}")
json=(-H 'Content-Type: application/json')

echo "== A attaches t1 by email -> 201 =="
curl -s -w '\nstatus %{http_code}\n' -X POST "${BASE}/api/trainees" "${auth_a[@]}" "${json[@]}" \
  -d '{"email":"t1@fp.com"}'

echo "== A attaches t1 again -> 409 =="
code -X POST "${BASE}/api/trainees" "${auth_a[@]}" "${json[@]}" -d '{"email":"t1@fp.com"}'; echo

echo "== A attaches unknown email -> 404 =="
code -X POST "${BASE}/api/trainees" "${auth_a[@]}" "${json[@]}" -d '{"email":"ghost@fp.com"}'; echo

echo "== A attaches a coach account -> 400 =="
code -X POST "${BASE}/api/trainees" "${auth_a[@]}" "${json[@]}" -d '{"email":"coachB@fp.com"}'; echo

echo "== A attaches t2 -> 201; roster now 2 =="
code -X POST "${BASE}/api/trainees" "${auth_a[@]}" "${json[@]}" -d '{"email":"t2@fp.com"}'; echo
echo "== A lists roster =="
curl -s "${BASE}/api/trainees" "${auth_a[@]}"; echo
echo "== B lists roster -> empty =="
curl -s "${BASE}/api/trainees" "${auth_b[@]}"; echo

echo
echo "== A builds its exercise library (2 exercises) =="
curl -s -o /tmp/fp_ex1.json -X POST "${BASE}/api/exercises" "${auth_a[@]}" "${json[@]}" \
  -d '{"name":"Back Squat","video_url":"https://www.youtube.com/watch?v=dQw4w9WgXcQ"}'
EX1=$(id_of /tmp/fp_ex1.json)
curl -s -o /tmp/fp_ex2.json -X POST "${BASE}/api/exercises" "${auth_a[@]}" "${json[@]}" \
  -d '{"name":"Bench Press"}'
EX2=$(id_of /tmp/fp_ex2.json)
echo "exercise ids: ${EX1}, ${EX2}"

# B's own exercise, to test cross-coach reference
curl -s -o /tmp/fp_exb.json -X POST "${BASE}/api/exercises" "${auth_b[@]}" "${json[@]}" \
  -d '{"name":"Bicep Curl"}'
EXB=$(id_of /tmp/fp_exb.json)

T1_ID=$(id_of /tmp/fp_T1.json)
T3_ID=$(id_of /tmp/fp_T3.json)

echo
echo "== A creates a plan for t1 with 2 items -> 201 =="
curl -s -o /tmp/fp_plan.json -w 'status %{http_code}\n' -X POST "${BASE}/api/plans" \
  "${auth_a[@]}" "${json[@]}" -d "{
    \"trainee_id\": ${T1_ID}, \"name\": \"Week 1 - Full Body\", \"notes\": \"3x/week\",
    \"items\": [
      {\"exercise_id\": ${EX1}, \"day_label\": \"A\", \"target_sets\": 5, \"target_reps\": 5, \"target_weight\": 100, \"rest_seconds\": 180},
      {\"exercise_id\": ${EX2}, \"day_label\": \"A\", \"target_sets\": 3, \"target_reps\": 8, \"video_url\": \"https://youtu.be/dQw4w9WgXcQ\"}
    ]}"
cat /tmp/fp_plan.json; echo
PLAN=$(id_of /tmp/fp_plan.json)
echo "plan id = ${PLAN}"

echo "== A gets the plan -> 200, 2 items, order_index 0 and 1 =="
curl -s "${BASE}/api/plans/${PLAN}" "${auth_a[@]}"; echo

echo "== A lists plans -> 200, 1 header (no items in list) =="
curl -s "${BASE}/api/plans" "${auth_a[@]}"; echo

echo "== B gets A's plan -> 404 =="
code "${BASE}/api/plans/${PLAN}" "${auth_b[@]}"; echo

echo
echo "== validation: plan for a non-roster trainee (t3) -> 403 =="
code -X POST "${BASE}/api/plans" "${auth_a[@]}" "${json[@]}" -d "{
  \"trainee_id\": ${T3_ID}, \"name\": \"x\", \"items\": [{\"exercise_id\": ${EX1}}]}"; echo

echo "== validation: item references B's exercise -> 403 =="
code -X POST "${BASE}/api/plans" "${auth_a[@]}" "${json[@]}" -d "{
  \"trainee_id\": ${T1_ID}, \"name\": \"x\", \"items\": [{\"exercise_id\": ${EXB}}]}"; echo

echo "== validation: empty items array -> 400 =="
code -X POST "${BASE}/api/plans" "${auth_a[@]}" "${json[@]}" -d "{
  \"trainee_id\": ${T1_ID}, \"name\": \"x\", \"items\": []}"; echo

echo "== validation: target_sets = 0 -> 400 =="
code -X POST "${BASE}/api/plans" "${auth_a[@]}" "${json[@]}" -d "{
  \"trainee_id\": ${T1_ID}, \"name\": \"x\", \"items\": [{\"exercise_id\": ${EX1}, \"target_sets\": 0}]}"; echo

echo "== validation: bad item video_url host -> 400 =="
code -X POST "${BASE}/api/plans" "${auth_a[@]}" "${json[@]}" -d "{
  \"trainee_id\": ${T1_ID}, \"name\": \"x\", \"items\": [{\"exercise_id\": ${EX1}, \"video_url\": \"https://vimeo.com/1\"}]}"; echo

echo
echo "== A updates the plan: rename + drop to 1 item -> 200 =="
curl -s -w '\nstatus %{http_code}\n' -X PUT "${BASE}/api/plans/${PLAN}" "${auth_a[@]}" "${json[@]}" -d "{
  \"trainee_id\": ${T1_ID}, \"name\": \"Week 1 - Lower\", \"items\": [{\"exercise_id\": ${EX1}, \"target_sets\": 4, \"target_reps\": 6}]}"

echo "== A assigns the plan -> 200, is_active true =="
curl -s -w '\nstatus %{http_code}\n' -X POST "${BASE}/api/plans/${PLAN}/assign" "${auth_a[@]}"

echo "== A creates + assigns a 2nd plan for t1; 1st should deactivate =="
curl -s -o /tmp/fp_plan2.json -X POST "${BASE}/api/plans" "${auth_a[@]}" "${json[@]}" -d "{
  \"trainee_id\": ${T1_ID}, \"name\": \"Week 2\", \"items\": [{\"exercise_id\": ${EX2}}]}"
PLAN2=$(id_of /tmp/fp_plan2.json)
code -X POST "${BASE}/api/plans/${PLAN2}/assign" "${auth_a[@]}"; echo
echo "-- plan 1 is_active (expect false):"
curl -s "${BASE}/api/plans/${PLAN}" "${auth_a[@]}" | sed -n 's/.*\("is_active":[a-z]*\).*/\1/p'
echo "-- plan 2 is_active (expect true):"
curl -s "${BASE}/api/plans/${PLAN2}" "${auth_a[@]}" | sed -n 's/.*\("is_active":[a-z]*\).*/\1/p'

echo
echo "== authz: trainee hits coach plan route -> 403 =="
code "${BASE}/api/plans" "${auth_t1[@]}"; echo
echo "== authz: no token -> 401 =="
code "${BASE}/api/plans"; echo
echo "== B updates A's plan -> 404 =="
code -X PUT "${BASE}/api/plans/${PLAN}" "${auth_b[@]}" "${json[@]}" -d "{
  \"trainee_id\": ${T1_ID}, \"name\": \"hijack\", \"items\": [{\"exercise_id\": ${EXB}}]}"; echo

echo
echo "== request log =="
grep -E ' -> [0-9]' "$LOG" | tail -40
