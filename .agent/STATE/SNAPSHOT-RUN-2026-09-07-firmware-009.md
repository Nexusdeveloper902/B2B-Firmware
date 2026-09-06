# STATE SNAPSHOT — after RUN-2026-09-07-firmware-009

## Overall Status
Reader firmware healthy and green — now verified twice independently
(RUN-2026-09-07-firmware-008 and this run agree on every claim). No
ESP32-CAM code exists here yet; TASK-008 (camera merge + capture
trigger) remains OPEN and untouched.

## Completed
- Second independent verification on main @ 33434f2: `pio test -e
  native` 68/68; `pio run -e esp32dev` SUCCESS; Bearer fix confirmed
  in EspApiClient.h source (explicit Authorization header, no
  setAuthorization); camera-absence re-confirmed (three envs only:
  esp32dev, esp32dev-mock, native). See RUN-2026-09-07-firmware-009.
- Cross-repo same date: B2B-Core pyramid 239/1-skip + e2e 24/24 +
  quality PASS (RUN-2026-09-07-audit-002); ESP32-CAM-CV pytest
  114/1-skip + build trap reproduced (RUN-2026-09-07-cv-audit-002).

## In Progress
- Nothing.

## Blocked
- Nothing. (Hardware-in-the-loop verification remains the owner's
  manual checklist by design — docs/MANUAL_VERIFICATION_CHECKLIST.md.)

## Known Problems
- TASK-008 gap (owner's recycling spec §6/§7/§8/§37): no esp32cam
  environment, no ENTER capture trigger, no capture→classify wiring.
- The reference repo's fresh-checkout build failure (missing
  secrets.h, no __has_include guard) is still unfixed THERE; this
  repo's own guard (TASK-001) must be applied to any merged sources.

## Important Current Facts
- Branch: main @ 33434f2 plus this run's append-only records. No
  firmware-source changes since f325b2e (TASK-007 merge).
- Trust anchor: `pio test -e native` → 68/68; `pio run -e esp32dev` →
  SUCCESS (verified twice, 2026-09-07).
- Device→backend auth is Bearer-only via bearerAuthorizationValue()
  (ADR-007); a 401 now genuinely means the key has no readers row.
- No CI configured in this repo (0 workflow runs) — verification is
  local-first by convention (ADR-003 mock-first, native host tests).
- PlatformIO toolchain cache (~/.platformio, espressif32 + native)
  makes re-verification ~23s on this host.
