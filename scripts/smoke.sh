#!/usr/bin/env bash
# End-to-end smoke test. Boots the server on a throwaway SQLite database and
# drives the whole coach -> trainee flow over HTTP, checking the status code of
# every step. Exits non-zero if any check fails. Run locally or from CI:
#
#   cmake --build --preset dev && scripts/smoke.sh
#   FITPLAN_BIN=./build/release/fitplan scripts/smoke.sh
set -u

BIN="${FITPLAN_BIN:-./build/dev/fitplan}"
PORT="${FITPLAN_PORT:-18099}"
BASE="http://127.0.0.1:${PORT}"
WORKDIR="$(mktemp -d /tmp/fitplan_smoke.XXXXXX)"
DB="${WORKDIR}/smoke.db"
LOG="${WORKDIR}/server.log"
FAILS=0

if [[ ! -x "$BIN" ]]; then
    echo "smoke: server binary not found at '${BIN}' (build first, or set FITPLAN_BIN)" >&2
    exit 2
fi

FITPLAN_DB_PATH="$DB" FITPLAN_PORT="$PORT" FITPLAN_JWT_SECRET=smoke-secret \
    FITPLAN_MIGRATIONS_DIR=src/db/migrations FITPLAN_LOG_LEVEL=warn \
    "$BIN" >"$LOG" 2>&1 &
SRV=$!
cleanup() {
    kill "$SRV" 2>/dev/null
    wait "$SRV" 2>/dev/null
    rm -rf "$WORKDIR"
}
trap cleanup EXIT

for _ in $(seq 1 50); do
    curl -fsS -o /dev/null "${BASE}/api/health" 2>/dev/null && break
    sleep 0.1
done
if ! curl -fsS -o /dev/null "${BASE}/api/health" 2>/dev/null; then
    echo "smoke: server never became ready; log follows" >&2
    cat "$LOG" >&2
    exit 1
fi

json=(-H 'Content-Type: application/json')

# req <body-outfile> <curl args...>  ->  prints the HTTP status code
req() {
    local out="$1"
    shift
    curl -s -o "$out" -w '%{http_code}' "$@"
}

# assert <label> <expected-code> <actual-code>
assert() {
    if [[ "$3" == "$2" ]]; then
        printf 'ok    %-40s %s\n' "$1" "$3"
    else
        printf 'FAIL  %-40s expected %s, got %s\n' "$1" "$2" "$3"
        FAILS=$((FAILS + 1))
    fi
}

token_of() { sed -n 's/.*"access_token":"\([^"]*\)".*/\1/p' "$1"; }
first_id() { grep -o '"id":[0-9]\+' "$1" | head -1 | grep -o '[0-9]\+'; }
nth_id() { grep -o '"id":[0-9]\+' "$2" | sed -n "${1}p" | grep -o '[0-9]\+'; }

B="${WORKDIR}/body.json" # scratch body, overwritten each request
CJ="${WORKDIR}/coach.json"
TJ="${WORKDIR}/trainee.json"
EJ="${WORKDIR}/exercise.json"
PJ="${WORKDIR}/plan.json"

echo "== health & auth =="
assert "GET  /api/health" 200 "$(req "$B" "${BASE}/api/health")"
assert "POST /api/auth/register (coach)" 201 "$(req "$CJ" "${json[@]}" \
    -X POST "${BASE}/api/auth/register" \
    -d '{"email":"coach@smoke.dev","password":"password123","role":"coach","display_name":"Coach"}')"
assert "POST /api/auth/register (trainee)" 201 "$(req "$TJ" "${json[@]}" \
    -X POST "${BASE}/api/auth/register" \
    -d '{"email":"trainee@smoke.dev","password":"password123","role":"trainee","display_name":"Trainee"}')"
assert "POST /api/auth/login (wrong pw)" 401 "$(req "$B" "${json[@]}" \
    -X POST "${BASE}/api/auth/login" \
    -d '{"email":"coach@smoke.dev","password":"nope"}')"
assert "GET  /api/auth/me (no token)" 401 "$(req "$B" "${BASE}/api/auth/me")"

CT="$(token_of "$CJ")"
TT="$(token_of "$TJ")"
TID="$(first_id "$TJ")"
if [[ -z "$CT" || -z "$TT" || -z "$TID" ]]; then
    echo "smoke: could not read tokens / trainee id from the register responses" >&2
    cat "$CJ" "$TJ" >&2
    exit 1
fi
authc=(-H "Authorization: Bearer ${CT}")
autht=(-H "Authorization: Bearer ${TT}")

echo "== coach: roster, library, plan =="
assert "POST /api/trainees (attach)" 201 "$(req "$B" "${authc[@]}" "${json[@]}" \
    -X POST "${BASE}/api/trainees" -d '{"email":"trainee@smoke.dev"}')"
assert "POST /api/exercises" 201 "$(req "$EJ" "${authc[@]}" "${json[@]}" \
    -X POST "${BASE}/api/exercises" \
    -d '{"name":"Back Squat","video_url":"https://www.youtube.com/watch?v=ultWZbUMPL8"}')"
EX="$(first_id "$EJ")"
assert "POST /api/plans" 201 "$(req "$PJ" "${authc[@]}" "${json[@]}" \
    -X POST "${BASE}/api/plans" \
    -d "{\"trainee_id\":${TID},\"name\":\"Smoke Week\",\"items\":[{\"exercise_id\":${EX},\"target_sets\":3,\"target_reps\":5,\"target_weight\":100}]}")"
PLAN="$(first_id "$PJ")"
PI1="$(nth_id 2 "$PJ")"
assert "POST /api/plans/<id>/assign" 200 "$(req "$B" "${authc[@]}" \
    -X POST "${BASE}/api/plans/${PLAN}/assign")"

echo "== trainee: plan, session, progress =="
assert "GET  /api/my/plan" 200 "$(req "$B" "${autht[@]}" "${BASE}/api/my/plan")"
assert "POST /api/my/sessions" 201 "$(req "$B" "${autht[@]}" "${json[@]}" \
    -X POST "${BASE}/api/my/sessions" \
    -d "{\"plan_id\":${PLAN},\"sets\":[{\"exercise_id\":${EX},\"plan_item_id\":${PI1},\"reps\":5,\"weight\":100},{\"exercise_id\":${EX},\"plan_item_id\":${PI1},\"reps\":5,\"weight\":100},{\"exercise_id\":${EX},\"plan_item_id\":${PI1},\"reps\":5,\"weight\":100}]}")"
assert "GET  /api/my/sessions" 200 "$(req "$B" "${autht[@]}" "${BASE}/api/my/sessions")"
assert "GET  /api/my/progress" 200 "$(req "$B" "${autht[@]}" "${BASE}/api/my/progress")"
assert "GET  /api/trainees/<id>/progress" 200 "$(req "$B" "${authc[@]}" \
    "${BASE}/api/trainees/${TID}/progress")"

echo "== authorization & validation =="
assert "trainee hits a coach route -> 403" 403 "$(req "$B" "${autht[@]}" "${BASE}/api/exercises")"
assert "coach hits a trainee route -> 403" 403 "$(req "$B" "${authc[@]}" "${BASE}/api/my/plan")"
assert "no token -> 401" 401 "$(req "$B" "${BASE}/api/my/progress")"
assert "unknown exercise in a set -> 400" 400 "$(req "$B" "${autht[@]}" "${json[@]}" \
    -X POST "${BASE}/api/my/sessions" -d '{"sets":[{"exercise_id":999999}]}')"

echo
if [[ "$FAILS" -eq 0 ]]; then
    echo "smoke: all checks passed"
else
    echo "smoke: ${FAILS} check(s) failed"
    echo "---- server log ----"
    cat "$LOG"
fi
exit $((FAILS > 0 ? 1 : 0))
