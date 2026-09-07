# TASK-009 — ESP32-CAM as one integrated station device

## Objective
Refactor the firmware so the `esp32cam` environment is the COMPLETE
station (camera + RC522 + presence + capture trigger + feedback +
network in one lifecycle) instead of a camera-only firmware, using the
bench-validated RC522 wiring (SS 13 / SCK 14 / MOSI 15 / MISO 2 /
RST 4). Eliminate the diverged `config.h` / `config_camera.h` pair.

## Requirements
1. `esp32cam` boots camera + RC522 + network as one `Station`
   (recoverable per-subsystem states, never FATAL-halt on one failure).
2. RC522 on the validated pins; RST stays on GPIO4 (never GPIO16 —
   PSRAM); buzzer stays absent while RST is on GPIO4.
3. ONE authoritative hardware definition per env (no contradictory
   duplicates); no SD init; camera map preserved.
4. RFID taps resolve identity via the existing presence tap endpoint;
   `awaiting_classification` + event id auto-captures+classifies in the
   same transaction (existing `classifyWithEvent` path, no new protocol).
5. ENTER/shutter stay bottle-first; `a`/`e`/`c`, pairing mode, console
   password, visualizer preserved. `esp32dev` reader target unbroken.
6. Default build targets the station (`esp32cam`); reader stays via
   `-e esp32dev` / `flash.sh --esp32`.
7. Docs carry ONE authoritative station wiring table (both languages).

## Constraints
- No new backend protocol; no secrets merged or leaked; no SD init;
  no giant main.cpp; reuse NfcReader/WifiService/ApiClient/
  PresenceCore/Feedback seams.

## Acceptance Criteria
- `pio run -e esp32dev -e esp32dev-mock -e esp32cam` → 3/3 SUCCESS.
- `pio test -e native` → all pass, incl. a compile-time pin-map test.
- Stale references (`RST 16`, buzzer-on-4, old pin maps,
  `config_camera.h`) gone except as explicit prohibitions.

## Out of Scope
- IR sensor hardware (still the CaptureTrigger seam's future).
- Bench end-to-end tap → classify run (owner hardware session).
- OTA / provisioning UI (existing non-goals).

## Status
COMPLETE (implemented 2026-09-07: commits `4f1f3e9` + `933e65f`;
verified independently by RUN-2026-09-07-firmware-011).
