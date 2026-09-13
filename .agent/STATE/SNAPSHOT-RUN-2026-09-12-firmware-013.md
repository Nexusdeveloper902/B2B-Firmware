# STATE SNAPSHOT — RUN-2026-09-12-firmware-013

## Overall Status
User-visible firmware surfaces rebranded to Pulse; uncommitted.

## Completed
- station.cpp page, serial banner, platformio.ini, main.cpp header,
  READMEs EN/ES, HARDWARE_SETUP EN/ES.

## Known Problems
- No PlatformIO here: compile not run this run (string-literal-only
  change; risk nil but bench-verify at next flash).

## Important Current Facts
- Internal `Presence` namespace kept (Core ADR-047 decision).
