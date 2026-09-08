# ADR-011

## Date
2026-09-08

## Context
The station's red LED (GPIO33) polarity was a hardcoded assumption
(`activeLow=true`) carrying a "confirm polarity" comment since the
board map was written. Genuine AI-Thinker boards are active-LOW, but
clone boards exist with inverted LED drivers — on those, every pattern
renders inverted and the LED idles SOLID ON (the owner's "LED lights
unexpectedly"). Meanwhile nothing pinned the pin map against GPIO4
(the onboard flash LED) creeping back into a driven role.

## Decision
Polarity becomes a board-config define: `PIN_STATION_LED_ACTIVE_LOW`
in include/config/esp32cam.h (default `1` = active-LOW, the genuine
AI-Thinker wiring). `Station::Station()` consumes it; flipping one
line adapts an inverted-polarity clone with zero logic change. The
pin map is pinned by compile-time asserts: LED==GPIO33, polarity ∈
{0,1}, no pin collision with the RC522 bus or the shutter, and no
station define may drive GPIO4 (flash LED). A pattern-level test
(`station_idle_patterns_are_mostly_off`) guarantees every idle state
is OFF-dominant so "mostly on" can never ship silently.

## Alternatives Considered
- Runtime polarity autodetect (blink, measure, decide) — rejected: no
  feedback channel exists to measure; adds boot complexity for a
  bench-soldered constant.
- Two builds (per-polarity env) — rejected: a define IS the cheaper
  version of that; envs multiply flashing mistakes (the wrong-target
  failure mode already bit this bench once).
- Keep the hardcoded true + docs only — rejected: the assumption is
  exactly what produced the solid-LED report.

## Reasoning
Board electrical constants belong in the board config next to the pin
map they describe; an operator facing an inverted clone flips ONE
documented line instead of reading C++ constructor code. The
compile-time asserts turn the two historical failure modes (GPIO4 as
RST, wrong pin collisions) into build failures instead of field
mysteries.

## Consequences
- Inverted-clone operators change `PIN_STATION_LED_ACTIVE_LOW` to `0`
  and reflash — documented in HARDWARE_SETUP (EN+ES) diagnostics
  tables.
- Any future pin-map change must survive the assert set in
  test_station_config.cpp by design.
- Wrong-target flashing (reader image on the CAM board) remains an
  operator error the docs address, not a firmware-solvable one.

## Status
ACTIVE
