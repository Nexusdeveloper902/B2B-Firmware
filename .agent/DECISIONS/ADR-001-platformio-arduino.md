# ADR-001: PlatformIO + Arduino framework on ESP32 (esp32dev)

## Status
Accepted (2026-09-05, TASK-001 Phase A)

## Context
The reader firmware must target an ESP32 with an RC522 NFC reader over
SPI. Candidate stacks: bare ESP-IDF (C), Arduino framework on ESP-IDF, or
PlatformIO managing an Arduino-framework project.

## Decision
PlatformIO project (`platformio.ini`) targeting `board = esp32dev` with
`framework = arduino`, platform `espressif32`. Libraries via `lib_deps`
(ArduinoJson ^7, miguelbalboa/rfid ^1.6.4 for the RC522).

Reasons:
- The master protocol names PlatformIO + Arduino explicitly as the target
  stack, and asks for `pio run` buildability as the meaning of "buildable".
- PlatformIO gives three environments from one project: `esp32dev` (real
  RC522), `esp32dev-mock` (serial-simulated reader — the default while
  hardware is absent), and `native` (host-side unit tests of pure logic).
- The Arduino ecosystem's RC522 and HTTP client libraries are mature; the
  task is intentionally minimal, so ESP-IDF's extra control is not needed.

## Consequences
- `pio run -e esp32dev-mock` is the default env (no board attached in
  agent runs); flashing real hardware uses `pio run -e esp32dev`.
- Host tests live in `test/` and link only the Arduino-free `lib/PresenceCore`.
- Upgrading the espressif32 platform major is a build-flag/dependency
  concern, not a rewrite.
