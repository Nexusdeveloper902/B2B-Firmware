# ADR-002: Boot-time mode selection via a physical pin

## Status
Accepted (2026-09-05, TASK-001 Phase C)

## Context
The reader has exactly two operating modes (Operation, Pairing). The task
spec requires: "Mode determined at boot by the mode-select button/pin state
(documented exact pin/logic level)". Options: runtime switching via serial
command, a config value in flash, or a physical pin read at boot.

## Decision
A mode-select GPIO (default GPIO 32) wired to GND through a momentary
button, read once at boot with `INPUT_PULLUP` and a 50 ms settle delay
(double-read + tie-break). Level mapping (documented in include/config.h
and docs/HARDWARE_SETUP.md):

- HIGH (button released) → OPERATION MODE
- LOW (button pressed and held during power-on/reset) → PAIRING MODE

The mode is fixed for the session; changing mode means pressing the button
and pressing EN/RESET.

## Rationale
- Exactly matches the task spec (boot-time, pin state, documented level).
- A physical control is verifiable by a human at the bench without serial
  tooling — it exercises the same code path a production enclosure button
  would.
- No flash config to corrupt, no serial protocol to get wrong during
  verification; reboot is an acceptable mode-switch cost for a reader that
  spends weeks in one mode.

## Consequences
- The MODE LED continuously indicates the selected mode so a human never
  has to guess which mode a running device is in.
- The mode-select read is debounced (two samples + settle) because a
  floating/oscillating pin at boot would otherwise randomize the mode.
- Remote/runtime mode reconfiguration remains an explicit non-goal.
