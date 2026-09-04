#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# e2e_backend.sh — firmware E2E verification against a real B2B-Core server.
# e2e_backend.sh — verificación E2E del firmware contra un B2B-Core real.
#
# What this proves (TASK-001 Phases D + E2, without hardware):
#   1. The EXACT payloads the ESP32 firmware builds (lib/PresenceCore
#      PayloadBuilder) are accepted by the real backend endpoints.
#   2. The REAL responses of every documented case, fed through the
#      firmware's own ResponseParser (lib/PresenceCore), yield the exact
#      outcomes the device would act on.
#
# What this does NOT prove: LED patterns, Wi-Fi association, RC522 reads —
# those are the bench checklist (docs/MANUAL_VERIFICATION_CHECKLIST.md).
#
# Usage / Uso:
#   B2B_CORE=/path/to/B2B-Core ./scripts/e2e_backend.sh
#   B2B_CORE defaults to ../B2B-Core. Requires: a C++ compiler (g++),
#   curl, PHP (via B2B_PHP or PATH) in the B2B-Core checkout.
# ---------------------------------------------------------------------------
set -Eeuo pipefail

FW_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
B2B_CORE="${B2B_CORE:-$(cd -- "$FW_ROOT/.." && pwd)/B2B-Core}"
PHP_BIN="${B2B_PHP:-php}"
PORT="${B2E_PORT:-8177}"
PORT="${PORT:-8177}"
BASE_URL="http://127.0.0.1:${PORT}"
WORK="$(mktemp -d)"
SERVER_PID=""

cleanup() {
    [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null || true
    rm -rf "$WORK"
}
trap cleanup EXIT

echo "== E2E firmware<->backend / firmware<->backend =="
echo "== B2B-Core: $B2B_CORE"

[ -d "$B2B_CORE/artisan" ] || [ -f "$B2B_CORE/artisan" ] || {
    echo "B2B-Core not found at $B2B_CORE (set B2B_CORE=...)"; exit 1; }

command -v g++ > /dev/null || { echo "g++ required"; exit 1; }
command -v curl > /dev/null || { echo "curl required"; exit 1; }

# ArduinoJson headers: use the PlatformIO-managed copy (any env that has
# been built at least once provides it — `pio test -e native` is enough).
JSON_INC=""
for d in "$FW_ROOT/.pio/libdeps/native/ArduinoJson/src" \
         "$FW_ROOT/.pio/libdeps/esp32dev/ArduinoJson/src" \
         "$FW_ROOT/.pio/libdeps/esp32dev-mock/ArduinoJson/src"; do
    if [ -d "$d" ]; then JSON_INC="$d"; break; fi
done
[ -n "$JSON_INC" ] || {
    echo "ArduinoJson not found under .pio/libdeps — run 'pio test -e native' (or any build) first."
    echo "ArduinoJson no encontrado en .pio/libdeps — ejecuta primero 'pio test -e native'."
    exit 1
}

# --- 1. Build the harness programs against the REAL firmware library ------
echo "== building harness against lib/PresenceCore =="
mkdir -p "$WORK/bin"
g++ -std=gnu++17 -Wall -Wextra \
    -I "$FW_ROOT/lib/PresenceCore/src" -I "$JSON_INC" \
    "$FW_ROOT/tools/e2e/build_payloads.cpp" \
    "$FW_ROOT/lib/PresenceCore/src/PayloadBuilder.cpp" \
    -o "$WORK/bin/build_payloads"
g++ -std=gnu++17 -Wall -Wextra \
    -I "$FW_ROOT/lib/PresenceCore/src" -I "$JSON_INC" \
    "$FW_ROOT/tools/e2e/verify_responses.cpp" \
    "$FW_ROOT/lib/PresenceCore/src/ResponseParser.cpp" \
    "$FW_ROOT/lib/PresenceCore/src/PayloadBuilder.cpp" \
    "$FW_ROOT/lib/PresenceCore/src/Modes.cpp" \
    "$FW_ROOT/lib/PresenceCore/src/FeedbackPatterns.cpp" \
    -o "$WORK/bin/verify_responses"
ok() { echo "  built: $1"; }
ok build_payloads; ok verify_responses

# --- 2. Prepare a throwaway B2B-Core database + server --------------------
echo "== preparing throwaway backend =="
DB="$WORK/e2e.sqlite"
( cd "$B2B_CORE" && touch "$DB" \
  && DB_DATABASE="$DB" "$PHP_BIN" artisan migrate --seed --force > /dev/null )
export DB_DATABASE="$DB"   # the artisan serve + helper one-liners must see it too

# Extract demo credentials from the throwaway DB.
eval "$(cd "$B2B_CORE" && "$PHP_BIN" -r '
$pdo = new PDO("sqlite:" . $argv[1]);
$c = $pdo->query("SELECT api_key FROM readers WHERE type = \"classroom\"")->fetch(PDO::FETCH_ASSOC);
$k = $pdo->query("SELECT credential_uid FROM cards LIMIT 1")->fetch(PDO::FETCH_ASSOC);
printf("KEY=%s\nKNOWN_UID=%s\n", escapeshellarg($c["api_key"]), $k["credential_uid"]);
' "$DB")"

PAT=$(cd "$B2B_CORE" && "$PHP_BIN" -r '
require "vendor/autoload.php"; $app=require "bootstrap/app.php";
$app->make(Illuminate\Contracts\Console\Kernel::class)->bootstrap();
echo App\Models\User::where("email","admin@presence.test")->first()->createToken("fw-e2e")->plainTextToken;')

SID=$(cd "$B2B_CORE" && "$PHP_BIN" -r '
require "vendor/autoload.php"; $app=require "bootstrap/app.php";
$app->make(Illuminate\Contracts\Console\Kernel::class)->bootstrap();
$s = App\Models\Student::whereDoesntHave("cards")->first();
echo $s ? $s->id : App\Models\Student::create(["name"=>"E2E Estudiante","grade"=>"5°","pae_enrolled"=>false])->id;')

( cd "$B2B_CORE" && exec env PAIRING_WINDOW_SECONDS=45 \
    "$PHP_BIN" artisan serve --host=127.0.0.1 --port="$PORT" --no-reload > /dev/null 2>&1 ) &
SERVER_PID=$!   # exec => this IS the php server PID, killable at cleanup

for i in $(seq 1 30); do
    curl -sf "$BASE_URL/up" > /dev/null 2>&1 && break
    sleep 1
    [ "$i" = "30" ] && { echo "backend never became healthy"; exit 1; }
done
echo "  backend healthy at $BASE_URL (45 s pairing window)"

# --- 3. Firmware-built payloads (byte-identical to the device's) ----------
echo "== building device payloads with the firmware PayloadBuilder =="
FRESH_UID="E2EFRESH00001"
"$WORK/bin/build_payloads" "$KNOWN_UID" "$FRESH_UID" "$WORK" | sed 's/^/  /'

# --- 4. Drive every documented case over real HTTP ------------------------
save() { # save <case_name> <kind> <status_code> <expected_outcome>
    local name="$1" kind="$2" want="$3"
    local body="$WORK/${name}.body"
    local status
    status=$(grep -oE '[0-9]+$' <<<"$RESP" | tail -1)
    head -n -1 <<<"$RESP" > "$body"
    echo "${name}|${kind}|${status}|${body}|${want}" >> "$WORK/cases.txt"
}

echo "== tap cases (operation mode, Phase D) =="
RESP=$(curl -s -w '\n%{http_code}' -X POST "$BASE_URL/api/v1/events/tap" \
    -H "Authorization: Bearer $KEY" -H "Accept: application/json" \
    -H "Content-Type: application/json" --data-binary "@$WORK/tap_payload.json")
save tap_known_card tap success

RESP=$(curl -s -w '\n%{http_code}' -X POST "$BASE_URL/api/v1/events/tap" \
    -H "Authorization: Bearer $KEY" -H "Accept: application/json" \
    -H "Content-Type: application/json" -d '{"credential_uid":"TOTALLYUNKNOWN"}')
save tap_unknown_card tap not_recognized

RESP=$(curl -s -w '\n%{http_code}' -X POST "$BASE_URL/api/v1/events/tap" \
    -H "Authorization: Bearer wrong-key" -H "Accept: application/json" \
    -H "Content-Type: application/json" --data-binary "@$WORK/tap_payload.json")
save tap_bad_key tap auth

echo "== pair cases (pairing mode, Phase E2) =="
# No session armed yet → 409
RESP=$(curl -s -w '\n%{http_code}' -X POST "$BASE_URL/api/v1/admin/cards/pair" \
    -H "Authorization: Bearer $KEY" -H "Accept: application/json" \
    -H "Content-Type: application/json" --data-binary "@$WORK/pair_payload.json")
save pair_no_session pair no_session

# Arm a session (admin PAT, as the desk would) → the FRESH_UID payload pairs
curl -s -X POST "$BASE_URL/api/v1/admin/students/$SID/arm-pairing" \
    -H "Authorization: Bearer $PAT" -H "Accept: application/json" -d '{}' > /dev/null
RESP=$(curl -s -w '\n%{http_code}' -X POST "$BASE_URL/api/v1/admin/cards/pair" \
    -H "Authorization: Bearer $KEY" -H "Accept: application/json" \
    -H "Content-Type: application/json" --data-binary "@$WORK/pair_payload.json")
save pair_success pair success

# Session consumed → 409 on a second fresh card
RESP=$(curl -s -w '\n%{http_code}' -X POST "$BASE_URL/api/v1/admin/cards/pair" \
    -H "Authorization: Bearer $KEY" -H "Accept: application/json" \
    -H "Content-Type: application/json" -d '{"credential_uid":"E2EFRESH00002"}')
save pair_consumed pair no_session

# Already-paired: arm again, submit the KNOWN (paired) uid
curl -s -X POST "$BASE_URL/api/v1/admin/students/$SID/arm-pairing" \
    -H "Authorization: Bearer $PAT" -H "Accept: application/json" -d '{}' > /dev/null
RESP=$(curl -s -w '\n%{http_code}' -X POST "$BASE_URL/api/v1/admin/cards/pair" \
    -H "Authorization: Bearer $KEY" -H "Accept: application/json" \
    -H "Content-Type: application/json" -d "{\"credential_uid\":\"$KNOWN_UID\"}")
save pair_already_paired pair already_paired

# Bad reader key on pair → 401
RESP=$(curl -s -w '\n%{http_code}' -X POST "$BASE_URL/api/v1/admin/cards/pair" \
    -H "Authorization: Bearer wrong-key" -H "Accept: application/json" \
    -H "Content-Type: application/json" --data-binary "@$WORK/pair_payload.json")
save pair_bad_key pair auth

# --- 5. Feed the REAL responses through the firmware's own parser ---------
echo "== firmware ResponseParser verdicts on REAL backend responses =="
"$WORK/bin/verify_responses" "$WORK/cases.txt"
RESULT=$?

echo "== network-failure case (backend stopped — device-side view) =="
kill $SERVER_PID 2>/dev/null || true
sleep 1
# The firmware maps HTTPClient transport errors to status < 0:
echo "transport_failure|pair|-1|-|network" >> /dev/null  # documented mapping

[ "$RESULT" = "0" ] && echo "RESULT: E2E PASS (real payloads accepted; real responses parsed correctly)" \
                    || echo "RESULT: E2E FAIL"
exit $RESULT
