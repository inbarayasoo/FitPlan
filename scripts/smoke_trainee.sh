#!/usr/bin/env bash
# Manual end-to-end check of the Step 5 trainee API + progress engine. Not part
# of the test suite. Walks: coach builds & assigns a plan -> trainee views it,
# logs two sessions, patches one -> both progress views -> the authz edges.
set -u
PORT=18085
DB=/tmp/fp_smoke_trainee.db
LOG=/tmp/fp_smoke_trainee.log
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
first_id() { grep -o '"id":[0-9]\+' "$1" | head -1 | grep -o '[0-9]\+'; }
nth_id() { grep -o '"id":[0-9]\+' "$2" | sed -n "${1}p" | grep -o '[0-9]\+'; }

# The verification code the server just "sent" to <email> (no Brevo key -> the
# LogEmailSender writes the email body to the server log at warn level).
code_for() { grep -A8 "to=<$1>" "$LOG" | grep -oE '^ +[0-9]{6}$' | tail -1 | tr -d ' '; }

reg() { # email role name -> registers + verifies, writes the verify response to
        # /tmp/fp_$3.json, echoes the token
  curl -s -o /dev/null -X POST "${BASE}/api/auth/register" \
    -H 'Content-Type: application/json' \
    -d "{\"email\":\"$1\",\"password\":\"password123\",\"role\":\"$2\",\"display_name\":\"$3\"}"
  curl -s -o "/tmp/fp_$3.json" -X POST "${BASE}/api/auth/verify-email" \
    -H 'Content-Type: application/json' \
    -d "{\"email\":\"$1\",\"code\":\"$(code_for "$1")\"}"
  token_of "/tmp/fp_$3.json"
}

C=$(reg coach@fp.com coach C)
T=$(reg trainee@fp.com trainee T)
OTHER=$(reg other@fp.com trainee OTHER)
authc=(-H "Authorization: Bearer ${C}")
autht=(-H "Authorization: Bearer ${T}")
autho=(-H "Authorization: Bearer ${OTHER}")
json=(-H 'Content-Type: application/json')

TID=$(first_id /tmp/fp_T.json)

curl -s "${authc[@]}" "${json[@]}" -X POST "${BASE}/api/trainees" -d '{"email":"trainee@fp.com"}' >/dev/null
curl -s -o /tmp/fp_ex1.json "${authc[@]}" "${json[@]}" -X POST "${BASE}/api/exercises" \
  -d '{"name":"Back Squat","video_url":"https://www.youtube.com/watch?v=ultWZbUMPL8"}'
EX1=$(first_id /tmp/fp_ex1.json)
curl -s -o /tmp/fp_ex2.json "${authc[@]}" "${json[@]}" -X POST "${BASE}/api/exercises" \
  -d '{"name":"Bench Press"}'
EX2=$(first_id /tmp/fp_ex2.json)

echo "== coach builds a plan for the trainee (2 items) =="
curl -s -o /tmp/fp_plan.json "${authc[@]}" "${json[@]}" -X POST "${BASE}/api/plans" -d "{
  \"trainee_id\": ${TID}, \"name\": \"Week 1\",
  \"items\": [
    {\"exercise_id\": ${EX1}, \"target_sets\": 3, \"target_reps\": 5, \"target_weight\": 100},
    {\"exercise_id\": ${EX2}, \"target_sets\": 2, \"target_reps\": 8}
  ]}"
PLAN=$(first_id /tmp/fp_plan.json)
PI1=$(nth_id 2 /tmp/fp_plan.json)   # 1st item id (header id is 1st match)
PI2=$(nth_id 3 /tmp/fp_plan.json)
echo "plan=${PLAN} item1=${PI1} item2=${PI2}"

echo "== coach assigns it -> 200 =="
code "${authc[@]}" -X POST "${BASE}/api/plans/${PLAN}/assign"; echo

echo "== trainee GET /api/my/plan -> 200, effective video_url on item 1 =="
curl -s "${autht[@]}" "${BASE}/api/my/plan"; echo

echo "== trainee logs session 1 (last week): 3x squat @100, 2x bench @60 =="
LW=$(date -u -d '-7 days' '+%Y-%m-%d 09:00:00')
code "${autht[@]}" "${json[@]}" -X POST "${BASE}/api/my/sessions" -d "{
  \"plan_id\": ${PLAN}, \"performed_at\": \"${LW}\",
  \"sets\": [
    {\"exercise_id\": ${EX1}, \"plan_item_id\": ${PI1}, \"reps\": 5, \"weight\": 100, \"rpe\": 8},
    {\"exercise_id\": ${EX1}, \"plan_item_id\": ${PI1}, \"reps\": 5, \"weight\": 100},
    {\"exercise_id\": ${EX1}, \"plan_item_id\": ${PI1}, \"reps\": 5, \"weight\": 100},
    {\"exercise_id\": ${EX2}, \"plan_item_id\": ${PI2}, \"reps\": 8, \"weight\": 60},
    {\"exercise_id\": ${EX2}, \"plan_item_id\": ${PI2}, \"reps\": 8, \"weight\": 60}
  ]}"; echo

echo "== trainee logs session 2 (today): 3x squat @102.5 =="
code "${autht[@]}" "${json[@]}" -X POST "${BASE}/api/my/sessions" -d "{
  \"plan_id\": ${PLAN},
  \"sets\": [
    {\"exercise_id\": ${EX1}, \"plan_item_id\": ${PI1}, \"reps\": 5, \"weight\": 102.5},
    {\"exercise_id\": ${EX1}, \"plan_item_id\": ${PI1}, \"reps\": 5, \"weight\": 102.5},
    {\"exercise_id\": ${EX1}, \"plan_item_id\": ${PI1}, \"reps\": 5, \"weight\": 102.5}
  ]}"; echo

echo "== trainee lists sessions =="
curl -s "${autht[@]}" "${BASE}/api/my/sessions"; echo
SID=$(curl -s "${autht[@]}" "${BASE}/api/my/sessions" | grep -o '"id":[0-9]\+' | head -1 | grep -o '[0-9]\+')

echo "== trainee PATCHes session ${SID}: status + notes -> 200 =="
curl -s -w '\nstatus %{http_code}\n' "${autht[@]}" "${json[@]}" -X PATCH \
  "${BASE}/api/my/sessions/${SID}" -d '{"status":"in_progress","notes":"tough day"}'

echo
echo "== trainee GET /api/my/progress =="
curl -s "${autht[@]}" "${BASE}/api/my/progress"; echo

echo "== coach GET /api/trainees/${TID}/progress (roster-scoped) =="
curl -s "${authc[@]}" "${BASE}/api/trainees/${TID}/progress"; echo

echo
echo "== authz: another trainee cannot see this trainee's coach view -> 403 =="
code "${autho[@]}" "${BASE}/api/trainees/${TID}/progress"; echo
echo "== authz: coach hits trainee-only route -> 403 =="
code "${authc[@]}" "${BASE}/api/my/plan"; echo
echo "== authz: no token -> 401 =="
code "${BASE}/api/my/progress"; echo
echo "== validation: unknown exercise in a set -> 400 =="
code "${autht[@]}" "${json[@]}" -X POST "${BASE}/api/my/sessions" \
  -d '{"sets":[{"exercise_id":99999}]}'; echo
echo "== validation: set links a plan item not on the plan -> 403 =="
code "${autht[@]}" "${json[@]}" -X POST "${BASE}/api/my/sessions" \
  -d "{\"sets\":[{\"exercise_id\":${EX1},\"plan_item_id\":424242}]}"; echo
echo "== a trainee with no plan: GET /api/my/plan -> 404 =="
code "${autho[@]}" "${BASE}/api/my/plan"; echo

echo
echo "== request log =="
grep -E ' -> [0-9]' "$LOG" | tail -40
