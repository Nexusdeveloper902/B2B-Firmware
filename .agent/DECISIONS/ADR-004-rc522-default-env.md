# ADR-004: Real RC522 reader as the default build environment

## Status
Accepted (2026-09-05, TASK-002)

## Context
ADR-003 established mock-first build envs with `esp32dev-mock` as the
PlatformIO default, reasoning that "the agent sandbox has no board
attached". TASK-001 shipped with that default. The first real user then
ran a plain `pio run -t upload`, flashed the mock build to a device that
DOES have an RC522 attached, and the boot banner reported
`Reader impl: MOCK (Serial input)` — the serial-input tap simulator was
running on real hardware.

Two real defects surfaced from that report:

1. The build default served the agent's convenience, not the device's
   purpose. Every human with the board in hand gets the wrong firmware
   unless they remember the `-e esp32dev` flag.
2. `src/main.cpp` printed the mock hint lines ("---- type a UID + Enter
   ... ----") in EVERY build, so even a correctly flashed real-reader
   build would look like it was asking for typed UIDs.

## Decision
1. `platformio.ini`: `default_envs = esp32dev` — the real RC522/SPI
   reader. A plain `pio run` and `pio run -t upload` now build and flash
   the real reader. The mock env stays fully supported but is explicitly
   opt-in: `pio run -e esp32dev-mock`.
2. The boot banner is now build-impl-specific: the RC522 build prints
   "---- present a card to the reader / presenta una tarjeta al lector
   ----"; only the mock build prints the "type a UID + Enter" hint.
3. `Rc522NfcReader` is hardened so "real reader by default" is also
   robust by default:
   - explicit SPI pins from `config.h` (`PIN_RC522_SCK/MISO/MOSI`),
     so non-default wiring is a config edit, not a code edit;
   - self-health tracking: a failed init or a reader that stops
     answering at runtime is retried on the
     `RC522_REINIT_INTERVAL_MS` (5 s) cadence — non-blocking, no
     reboot needed (fix the wiring with the device running);
   - diagnostics through an injected `Print*` (Serial in production):
     `[NFC] RC522 detected — firmware version 0x92 / detectado` on
     success (positive confirmation the radio is alive), and
     `[NFC] RC522 NOT responding (version 0x..) — check wiring (SCK 18 /
     MISO 19 / ...) + power 3.3 V` on failure.

## Rationale
- The default build should serve the artifact's purpose (a physical
  NFC reader), not the development environment's limitation. Agent
  runs that lack hardware can still opt into the mock env explicitly.
- The mock-first default was an agent-ergonomics decision leaking into
  the end-user product; ADR-003's interface split remains intact and
  unchanged — only which env is the default flipped.
- Self-recovery + a positive detection line turn the first bench
  session from "is it working?" into a diagnosable, self-healing
  setup.

## Consequences
- Anyone flashing with plain `pio run -t upload` now gets the real
  reader. Documentation (README + HARDWARE_SETUP, EN and ES) states
  this prominently.
- The mock env's UX is unchanged for development without a reader.
- ADR-003 is amended in part: mock-first remains the *development
  workflow* for board-less runs, but real-hardware is the *build
  default*. The ADR-003 decision text records the historical default
  (`esp32dev-mock`), superseded here.
