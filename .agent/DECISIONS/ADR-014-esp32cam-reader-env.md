# ADR-014

## Date
2026-09-15

## Context
The owner wants an AI-Thinker ESP32-CAM board to act purely as the NFC
reader — no camera connected, no capture logic — with the RC522 wired on
the same pins as the station. Neither existing image fits: `esp32cam`
is the station (camera init, shutter, capture flow), and `esp32dev` on a
CAM board is the documented wrong-target failure (RC522 on 18/19/23/5,
LEDs on 25/26 = camera bus, buzzer idling GPIO33 LOW → red LED solid ON).

## Decision
New opt-in env `esp32cam-reader`: `board = esp32cam`, builds
`src/main.cpp` only (no `camera/` sources, no `esp32-camera` lib),
flags `-DPRESENCE_READER_IMPL_RC522 -DREADER_ON_CAM_BOARD`,
`monitor_dtr/rts = 0`. `config.h` dispatches it to
`config/esp32cam_reader.h`. The CAM board pins (RC522 + GPIO33 LED +
polarity) moved from `esp32cam.h` into `config/esp32cam_board.h`,
included by both CAM configs — one definition, so the reader's RC522
map cannot drift from the station's. Feedback uses the existing
one-pin `StationLed` (events preempt the mode heartbeat); no buzzer.
The default env stays `esp32cam` (ADR-010 unchanged).

## Alternatives Considered
- Runtime "camera absent" branch inside the station — rejected: the
  owner wants no camera logic on the device; the station's capture
  state machine and `esp32-camera` would still ship.
- Duplicating the RC522 defines in a new header — rejected: two
  copies of bench-verified pins is exactly how they drift.
- A new two-pin feedback controller (GPIO33 + GPIO4 flash LED) —
  rejected: GPIO4 is never driven (ADR-011 / TASK-010 invariant).

## Consequences
- `test_cam_reader_config.cpp` pins the map at compile time: station
  SPI pins, RST un-driven, LED 33, and `#error` if the DevKit
  LED/buzzer, shutter or camera-bus defines leak in.
- `scripts/flash.sh --cam-reader` / `--board cam-reader`.
- Bench verification of the image on real hardware still pending.
