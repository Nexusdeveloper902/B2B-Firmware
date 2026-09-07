# STATE SNAPSHOT — after RUN-2026-09-07-firmware-011

## Overall Status
TASK-009 COMPLETE and independently verified: `esp32cam` is the
unified station (camera + RC522 + presence + trigger + feedback +
network, recoverable subsystems); reader DevKit preserved; default
build is the station. 3/3 envs SUCCESS, native 92/92, dispatcher flag
and secrets guards re-proven this run. Branch main @ `933e65f`,
pushed (pending: this run's memory commit).

## Completed
- Station refactor (`4f1f3e9`): Station lifecycle, config dispatcher
  (`config/{common,esp32dev,esp32cam}.h`, `config_camera.h` deleted),
  auto-classify tap transaction, StationLed (GPIO33), recoverable
  camera/NFC/Wi-Fi, default `esp32cam` + flash.sh, 92/92 tests.
- Bench bring-up (`d657265`): verified RC522 map 13/14/15/2/4, buzzer
  -1, per-board headers/secrets, shutter/buzzer seam.
- Verification pass (this run): full rebuild + retest + flag-routing
  proof + fresh-checkout proof + stale sweep + full-diff review.
- Memory: TASK-009, ADR-009, ADR-010 (supersedes ADR-004 §Decision-1),
  RUN-011 record, this snapshot.

## In Progress
- Memory commit + push for this run's records.

## Blocked
- Nothing. Owner bench end-to-end session remains the only hardware
  gate (documented script in CAMERA_STATION.md).

## Known Problems
- GPIO33 LED polarity assumed active-LOW (cosmetic; serial is truth).
- Full-load 3V3 rail dip never metered.
- Reference repo ESP32-CAM-CV fresh-checkout failure (external,
  carried over from snapshot-010).

## Important Current Facts
- Default env is `esp32cam` (ADR-010). DevKit operators must pass
  `-e esp32dev` / `flash.sh --esp32`.
- Station pins: RC522 13/14/15/2/4, shutter 12→GND, LED 33, buzzer
  -1, PSRAM 16/17 reserved, no SD. Pinned by
  `test_station_config.cpp` static_asserts.
- Trust anchor: native 92/92; 3/3 SUCCESS; dispatcher + guards proven
  2026-09-07 (RUN-011).
