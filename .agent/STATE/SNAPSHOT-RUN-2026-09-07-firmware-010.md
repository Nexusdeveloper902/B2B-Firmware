# STATE SNAPSHOT — after RUN-2026-09-07-firmware-010

## Overall Status
TASK-008 COMPLETE: the camera station exists. Four build environments
(esp32dev, esp32dev-mock, esp32cam, native) all green; 86/86 host
tests; reader functionality untouched. The recycling flow's device side
now has both halves: tap (reader) and image (camera) — both talking to
the B2B-Core TASK-025 backend landed the same date.

## Completed
- ESP32-CAM merge from the verified reference (init/pin map/capture/
  visualizer) as [env:esp32cam], monitor_dtr/rts=0 preserved.
- CaptureTrigger seam (spec §37): TerminalCaptureTrigger (line
  discipline + cooldown, host-tested); ENTER is the temporary trigger.
- Upload wiring: ENTER → POST /api/v1/recycling/capture (bottle-first);
  'a <uid>' → associate; 'e <event_id>' → card-first classify on next
  ENTER; 'c' → local capture. Multipart + Bearer from host-tested core.
- Bench documentation: docs/CAMERA_STATION.md + .es.md (exact commands
  + expected output vs a running B2B-Core).
- ADR-008 (upload endpoint decision + the seam).

## In Progress
- Nothing.

## Blocked
- Nothing. Hardware bench verification is the owner's (documented
  script), by the repo's hardware-honesty convention.

## Known Problems
- The reference repo (ESP32-CAM-CV) still fails a fresh checkout build
  (no __has_include guard) — being fixed there this same date as part
  of its TASK-001 resolution; THIS repo's merged sources carry the
  guard by construction.

## Important Current Facts
- Branch: main (feature/TASK-008-esp32cam-capture-trigger merged,
  commit cfdfa3c). src/ holds two mains: src/main.cpp (reader envs,
  filtered +<main.cpp>) and src/camera/main.cpp (esp32cam, filtered
  +<camera/>).
- Trust anchor: `pio test -e native` → 86/86; `pio run -e esp32dev -e
  esp32dev-mock -e esp32cam` → 3/3 SUCCESS.
- Camera serial commands: ENTER / a <uid> / e <event_id> / c.
- The Arduino envs compile C++11: default-member-initializer structs
  need explicit constructors for brace-init (recorded in the run
  record's Discoveries).
- Demo sharing mode: one recycling reader key serves both the RC522
  station and the camera station (documented; production should
  provision dedicated rows).
