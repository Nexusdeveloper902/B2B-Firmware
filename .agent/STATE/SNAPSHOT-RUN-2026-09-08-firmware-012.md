# STATE SNAPSHOT — after RUN-2026-09-08-firmware-012

## Overall Status
TASK-010 COMPLETE: the station LED's "unexpectedly on" modes are
diagnosed, fixed where firmware can fix them, pinned by compile-time
asserts, and documented bilingually. Branch main @ <this run's TASK-010
commit> (pushed; see RUN record). Native 93/93, esp32cam build
SUCCESS.

## Completed
- LED polarity as board config: `PIN_STATION_LED_ACTIVE_LOW`
  (ADR-011) — genuine AI-Thinker default 1, inverted clones flip one
  line.
- Pin map pinned: LED=33, no collisions with RC522/shutter, GPIO4
  (flash LED) may never appear as a driven station pin.
- Idle patterns proven OFF-dominant (mostly-on can never ship
  silently).
- Bilingual diagnostics tables (HARDWARE_SETUP + CAMERA_STATION):
  white solid → GPIO4/RST wiring + reflash; red solid → wrong-target
  image or polarity clone; red blips → normal grammar.

## Verification
- `pio test -e native` 93/93 (new: station_idle_patterns_are_mostly_off)
- `pio run -e esp32cam` SUCCESS
- Toolchain: PlatformIO 6.2.0 provisioned in-sandbox (pip --user) —
  native tests + station build run without sudo.

## Hardware bench pending (unchanged from snapshot-011)
The full CAMERA_STATION.md bench flow (capture → tap → classify →
pairing) still needs the physical board session; the LED fix is
flash-verified, not LED-verified (no board in this sandbox).

## Next
- Owner: hardware bench for the station (docs/CAMERA_STATION.md);
  observe the LED grammar live.
- B2B-Core TASK-027 (same session) closed the backend punch list;
  coordinated deploy = reflash station + `./run setup && ./run serve`.
