# ADR-003: Mock-first reader development + split build environments

## Status
Accepted (2026-09-05, TASK-001 Phases B/G)

## Context
Most agent runs (and this one) execute without the physical ESP32 board,
RC522 module, LEDs or buttons. The protocol demands the mode/API logic
still be exercised, honestly bounded: compilation + host tests +
mock-reader-triggered behavior, never claimed as hardware verification.

## Decision
1. `NfcReader` is a pure interface (`lib/NfcReader/src/NfcReader.h`) with
   two implementations selected at BUILD time, not runtime:
   - `esp32dev` env → `Rc522NfcReader` (MFRC522 over SPI)
   - `esp32dev-mock` env (default) → `MockSerialNfcReader` (type a UID +
     Enter in the Serial Monitor → identical tap pipeline)
2. All hardware-independent rules (JSON payloads, response parsing, mode
   strategies, debounce, LED pattern tables) live in `lib/PresenceCore`
   with ZERO Arduino includes, unit-tested in the `native` env
   (`pio test -e native`).
3. `src/main.cpp` is a thin composition root: it wires interfaces and
   contains no business logic.

## Rationale
- Build-time selection keeps the device image free of dead code and makes
  each env honest: mock env flashes to a real board too (serial-driven),
  which is how a bench human drives pairing tests without an RC522 wired.
- The native env cannot compile Arduino headers, so the discipline
  "hardware-independent logic must be host-testable" is enforced by the
  build system, not by convention.

## Consequences
- Documentation states plainly which verification happened (compile/host
  tests/mock behavior) and which requires the bench checklist.
- Adding a future NFC chip (PN532, etc.) is a new NfcReader implementation
  plus a new platformio.ini env — nothing else changes.
