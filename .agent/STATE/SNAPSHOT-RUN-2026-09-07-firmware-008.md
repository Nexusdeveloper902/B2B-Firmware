# STATE SNAPSHOT — after RUN-2026-09-07-firmware-008

## Overall Status
Firmware healthy and truthful; audit green; camera merge tracked as
TASK-008 (OPEN). No source changes this run.

## Completed
- TASK-007 claims independently re-verified: native 68/68 PASSED,
  esp32dev build SUCCESS, Bearer fix present in EspApiClient.h.
- Audit records appended (RUN-2026-09-07-firmware-008, TASK-008,
  this snapshot).

## In Progress
- Nothing.

## Blocked
- Nothing.

## Known Problems
- No ESP32-CAM code exists in this repo (recycling spec §6/§7 gap —
  TASK-008). No device-side classify caller anywhere.
- The reference camera firmware (ESP32-CAM-CV) fails a fresh-checkout
  build until include/secrets.h is copied from the .example (no
  __has_include guard). Merge must fix that (TASK-008 acceptance).

## Important Current Facts
- Branch: main @ f325b2e (TASK-007 merge).
- Environments: esp32dev (RC522, default), esp32dev-mock, native.
- Trust anchors re-run 2026-09-07: `pio test -e native` → 68/68;
  `pio run -e esp32dev` → SUCCESS.
- Device→backend auth: explicit `Authorization: Bearer <key>` header
  (ADR-007; never setAuthorization — the Basic-prefix trap).
- Companion records: B2B-Core RUN-2026-09-07-audit-001 + TASK-025;
  ESP32-CAM-CV .agent/ (created by the same audit run).
