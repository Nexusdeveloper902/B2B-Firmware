# TASK-010 — station LED: polarity as config, pin map pinned, diagnostics documented

## Date opened
2026-09-08

## Origin
Owner item (TASK-027 list, B2B-Core): "ESP32-CAM built-in LED lights
unexpectedly — diagnose the cause and fix."

## Diagnosis (root-cause, from the tree + history)
The station has TWO LEDs and exactly three "unexpectedly on" modes:

1. **White flash LED (GPIO4) solid ON** — the pre-e75e190 wiring used
   GPIO4 as RC522 RST; the MFRC522 library holds RST HIGH after every
   `PCD_Init` (re-run every 5 s by the re-init poll) → the flash LED
   burns. ALREADY fixed at e75e190 (RST strapped to 3V3, pin -1), but
   nothing PINNED it: a stale board image or an old RST wire still
   reproduces it, and a refactor could silently re-introduce GPIO4.
2. **Red LED (GPIO33) SOLID ON** — two causes: (a) the esp32dev
   READER image flashed on the CAM board (that image idles GPIO33 LOW
   for a buzzer; the red LED is active-LOW → solid ON — documented as
   the "original wrong-target failure"); (b) inverted-polarity clone
   boards, where the hardcoded `activeLow=true` assumption inverts
   every pattern into "mostly on".
3. **Red LED short blips** — NOT a fault: the heartbeat grammar
   (1=operation, 2=pairing, 3=degraded) reads as "random blinks" to
   anyone without the reference.

## Fix (this task)
- `PIN_STATION_LED_ACTIVE_LOW` config define (default 1 = genuine
  AI-Thinker active-LOW; one line flips an inverted clone) — wired
  into Station::Station() instead of the buried `true`.
- Compile-time pin invariants in test_station_config.cpp: LED pin ==
  33, polarity define ∈ {0,1}, no collision LED↔RC522-bus↔shutter, and
  "GPIO4 is never a driven pin" across every station define.
- `station_idle_patterns_are_mostly_off`: every idle pattern proven
  OFF-dominant (quiet gap ≥ 1400 ms) — "mostly on" can never ship
  silently.
- Bilingual diagnostics tables (HARDWARE_SETUP + CAMERA_STATION, EN +
  ES): symptom → root cause → fix for all three modes, plus the
  polarity-define pointer.

## Verification
- `pio test -e native` → **93/93** (was 92; +1 the new pattern test)
- `pio run -e esp32cam` → SUCCESS (station image builds with the
  config-driven polarity)

## Status
DONE — delivered by RUN-2026-09-08-firmware-012
