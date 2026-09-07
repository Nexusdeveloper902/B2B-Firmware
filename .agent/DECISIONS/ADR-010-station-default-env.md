# ADR-010: station (`esp32cam`) as the default build environment

## Date
2026-09-07

## Context
ADR-004 made `esp32dev` the default when the only real hardware was
the DevKit reader. The validated hardware is now the ESP32-CAM station,
and an `esp32dev` binary flashed to a CAM board drives camera-bus pins
as SPI — the exact wrong-target failure diagnosed 2026-09-07.

## Decision
`default_envs = esp32cam`; `scripts/flash.sh` defaults to the station.
Reader (`-e esp32dev` / `--esp32`) and mock (`-e esp32dev-mock` /
`--mock`) stay fully supported and explicitly opt-in.

## Alternatives Considered
- Keep `esp32dev` default: rejected — the primary bench is the
  station, and the default must serve the artifact's purpose (same
  rationale as ADR-004, applied to the new primary).

## Reasoning
Direct application of ADR-004's own principle to the current primary
hardware. No code path changes with the flag; only which env a bare
`pio run` / `./scripts/flash.sh` selects.

## Consequences
- ADR-004's default-env clause is SUPERSEDED (rest of ADR-004 stands:
  real-over-mock defaulting, banner discipline, RC522 self-recovery).
- Bare `pio run -t upload` on a DevKit now flashes the station image —
  DevKit operators must pass `-e esp32dev` (documented in
  platformio.ini, flash.sh usage, and HARDWARE_SETUP.md context).

## Status
ACTIVE (supersedes ADR-004 §Decision-1 only)
