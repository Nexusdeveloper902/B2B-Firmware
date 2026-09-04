# TASK-001-reader-firmware-mvp

Build a basic, working ESP32 firmware for a Presence Platform NFC reader
with exactly two operating modes:

1. **Pairing Mode** — scan a card, associate its UID with a student in the
   backend database, via the pairing endpoint built in `b2b-core` as its own
   TASK-010 (Phase E1 of this task; see the master protocol Section 0.3).
2. **Operation Mode** — scan a card, log a presence-event tap against the
   backend, show feedback.

PlatformIO project for ESP32 (Arduino framework, esp32dev). Bilingual
(EN/ES) real documentation in `docs/` — not only `.agent/`. Intentionally
minimal otherwise: no camera/recycling flow, no OTA, no captive-portal
provisioning. Get the core loop of both modes solid before anything else.

Phases: A (scaffolding) → B (NFC abstraction + mock) → C (mode system) →
D (operation mode) → E1 (b2b-core pairing endpoint, tracked there) →
E2 (pairing mode firmware) → F (resilience & feedback) → G (tests & docs).

---

## Commits

## Commit — d467771 (repo genesis, on main)
Date: 2026-09-05
Branch: main (genesis) → feature/TASK-001-reader-firmware-mvp continues

Summary: Phases A–D + F + G-firmware in one verified unit (a brand-new
empty repository has no prior main to protect; the genesis commit IS the
first buildable state, the feature branch carries the E1-dependent tail).
Scaffold (platformio.ini with esp32dev / esp32dev-mock / native envs,
config.h, secrets.h.example + gitignored secrets.h), NfcReader interface
+ Rc522NfcReader + MockSerialNfcReader, Mode strategy pair
(OperationMode/PairingMode in the Arduino-free PresenceCore), ApiClient +
EspApiClient + WifiService (bounded, non-blocking), LedFeedbackController
with distinct patterns for every outcome, CardDebouncer
(antenna-resting-safe), 48 host-side unit tests, bilingual README +
HARDWARE_SETUP + API_INTEGRATION + MANUAL_VERIFICATION_CHECKLIST, .agent/
PROJECT + ADR-001/002/003 + REQUIREMENTS + ARCHITECTURE + OBS-001.

Verification: `pio run -e esp32dev` PASS · `pio run -e esp32dev-mock` PASS
· `pio test -e native` 48/48 PASS.

## Commit — (this branch, Phase E2 + closure)
Date: 2026-09-05
Branch: feature/TASK-001-reader-firmware-mvp

Summary: Phase E2 backend integration E2E harness —
`tools/e2e/build_payloads.cpp` + `tools/e2e/verify_responses.cpp` +
`scripts/e2e_backend.sh`: sends the firmware's byte-identical payloads
(its own PayloadBuilder) to a real running B2B-Core (TASK-010 endpoints,
merged 667e4dd), captures every documented response case, and feeds the
REAL responses through the firmware's own ResponseParser. README(.es)
updated with the harness + layout. Run record + state snapshot + this
closure appended.

Verification: `./scripts/e2e_backend.sh` → **8/8 verdicts** (tap:
success/404/401; pair: success/409-consumed/422-already-paired/401;
pair_no_session 409) against B2B-Core main @ 667e4dd+ with a throwaway
seeded DB and real HTTP.

---

## Acceptance criteria — evaluation (2026-09-05)

```text
Phase A
[x] PlatformIO project scaffolded, compiles for esp32dev target
    — pio run -e esp32dev SUCCESS (also esp32dev-mock)
[x] secrets.h.example committed, secrets.h gitignored
    — template committed; git check-ignore verified; no real values anywhere
[x] config.h documents RC522 pin assignments with a wiring-confirmation note
    — include/config.h + the ⚠ note + docs/HARDWARE_SETUP.md(.es)
[x] Wi-Fi connects on boot with basic reconnect handling
    — WifiService: bounded 15 s connect + 10 s-cadence background reconnect;
      logic compiled for target; radio-level behavior = bench checklist §1/§8

Phase B
[x] NfcReader interface defined; RC522 implementation present
    — lib/NfcReader/src/NfcReader.h + Rc522NfcReader.h (MFRC522 lib)
[x] MockSerialNfcReader implemented and usable for testing without hardware
    — type UID+Enter in Serial Monitor; default esp32dev-mock env

Phase C
[x] Mode determined at boot from a documented pin/button state
    — GPIO32 INPUT_PULLUP, LOW=pairing at boot, 50 ms settle (ADR-002)
[x] Status LED continuously indicates current mode
    — MODE LED loops forever: 1 blip/2 s (operation) vs double blip (pairing)
[x] Mode logic implemented as swappable strategy classes
    — Mode interface + OperationMode/PairingMode (lib/PresenceCore/Modes)

Phase D
[x] Operation-mode tap call implemented against the CONFIRMED b2b-core contract
    — verified against routes/api.php + TapEventController, then re-verified
      against the LIVE backend (E2E harness: real 200/404/401 responses)
[x] Distinct feedback for success / not-recognized / auth-failure / network-failure
    — EVENT LED patterns + serial logs; every case host-tested + E2E-verified
[x] Device recovers and remains responsive after any failure case
    — non-blocking loop, bounded timeouts, no crash paths; parser tests cover
      garbage/empty bodies; physical recovery = bench checklist §3/§8

Phase E1 (in b2b-core, TASK-010 — COMPLETED, see its own task file)
[x] b2b-core's next task number determined by inspection, not guessed
    — TASK-009 was highest → TASK-010 created
[x] pending_pairings migration created
[x] POST /api/v1/admin/students/{id}/arm-pairing implemented per spec
[x] POST /api/v1/admin/cards/pair implemented per spec, incl. duplicate-card
    and no-active-session rejection
[x] Feature tests cover: happy path, no active session, already-paired card,
    expired session — 14 tests, all green
[x] docs/API.md (+.es.md) and Postman collection updated in b2b-core
[x] ADR recorded in b2b-core — ADR-020
[x] b2b-core's own main verified independently after merge
    — fresh checkout: composer install + migrate + 141 tests passed; pushed;
      CI workflow dispatched (run 33927676024)

Phase E2 (firmware)
[x] Pairing-mode HTTP call implemented against the now-live endpoint
    — PairingMode → POST /api/v1/admin/cards/pair, Bearer reader key
[x] Distinct feedback for pairing success / no-active-session /
    already-paired / network-failure
    — EVENT LED: solid / 3 blinks / 4 blinks / 5 fast blinks (+ serial log)
[x] End-to-end tested by arming a session via curl/Postman and confirming
    firmware behavior
    — scripts/e2e_backend.sh: real arm (admin PAT) + real pair with the
      firmware's payloads + real responses through the firmware parser,
      8/8 verdicts (bounded per Section 0.1: no physical board here —
      LED/RC522/Wi-Fi radio behavior is the bench checklist)

Phase F
[x] Card-read debouncing implemented
    — CardDebouncer: resting card = exactly one event; re-tap after absence
      + cooldown; millis-wrap-safe (7 dedicated host tests)
[x] Main loop uses non-blocking timing
    — zero delay() in loop(); pattern stepping + wifi.tick + poll
[x] FeedbackController interface separates LED/buzzer logic from business logic
    — modes emit FeedbackSignal only; LedFeedbackController renders

Phase G
[x] Host-side unit tests exist and pass
    — pio test -e native: 48/48 (payloads, every response case, mode
      strategies + feedback mapping, debounce + patterns)
[x] docs/HARDWARE_SETUP.md (+.es.md) complete
    — BOM, wiring tables, pattern reference, flashing, libraries, timing
[x] docs/MANUAL_VERIFICATION_CHECKLIST.md (+.es.md) complete, covering every
    scenario listed in the task spec
    — 9 sections / 27 items incl. tap→dashboard, arm→pair→DB, already-paired,
      409 no-session, expiry, Wi-Fi outage+recovery, power-cycle stability
```

Honesty boundary (Section 0.1): verified here = compilation for the real
target, host unit tests, mock-reader design, and real-HTTP integration
E2E. NOT verified here = RC522 radio reads, Wi-Fi association on real
hardware, LED/button physics — those are the bench checklist's job.

## Confirmed backend contract (verified in b2b-core main, not assumed)

- Tap: `POST /api/v1/events/tap`, `Authorization: Bearer <reader.api_key>`,
  body `{"credential_uid": "...", "client_timestamp": "..." (optional)}`.
  200 → `{status, event_id, event_type, student_first_name, next_step}`;
  404 → unknown/inactive card; 401 → bad key; 422 → validation.
  (routes/api.php + TapEventController, verified 2026-09-05.)
- Pairing (b2b-core TASK-010, merged 667e4dd): `POST /api/v1/admin/cards/pair`
  with the same Bearer reader key, body `{"credential_uid": "..."}`.
  200 → `{status, paired_student_name (full name), student_id}`; 409 → no
  active session; 422 → card already paired; 401 → bad key.
  Arming: `POST /api/v1/admin/students/{id}/arm-pairing` (admin session or
  PAT) → `{status, student_id, expires_at}` (45 s window, ADR-020).

## Out-of-scope guard

No camera/recycling flow, no OTA, no captive portal, no remote reader-mode
reconfiguration, no marketplace-repo changes, and b2b-core touched ONLY
via TASK-010 — all respected. Deferred to b2b-core follow-ups there.
