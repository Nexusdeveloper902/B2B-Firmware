# ADR-012 — Serial dispatch precedence + station self-healing (audit pass, 2026-09-09)

## Context

The full-stack audit found the camera station's documented serial commands
(`a <uid>`, `e <event_id>`, `c`) were **unreachable**: `dispatchSerialLine`
handed every non-empty line to the mode-password console FIRST, and
`ModeConsole::handleLine` treats any non-empty line as a password attempt —
so `a ABC123` printed "wrong password", poisoned the lockout counter, and
never reached the capture-command parser. The trigger's own design comment
("unknown line — including the mode password") shows the intended
grammar-first split; the station's dispatcher simply had the order inverted.

## Decisions

1. **Command grammar first, password console second.**
   `TerminalCaptureTrigger::feedLine(line)` (new, host-tested) feeds a whole
   line through the capture grammar; a `None` result is the dispatcher's
   signal to hand the line to `ModeConsole`. A mode password that collides
   with the command shapes (`c`, `a …`, `e …`) is consumed as the command —
   documented; operators pick a password outside those shapes.

2. **Card-first auto-capture (uncommitted WIP) hardened to its stated
   contract:** the classify upload failure now KEEPS the armed event
   (BUTTON/ENTER retryable, bounded by the 90 s window) instead of silently
   consuming it; a re-tap of the SAME event no longer re-arms (no reset of
   the auto one-shot on an event that may already be classified); `e 0` and
   absurd ids are rejected bilingually instead of arming a dead state.

3. **Associate 404s are distinguished.** The backend 404s for BOTH
   "capture gone" and "unknown/inactive card". Only the former (plus
   `already_associated`) clears the pending capture; a mistyped
   `a <uid>` keeps the window alive — the operator taps the right card,
   exactly like a physical tap rejection.

4. **Runtime peripheral death is now detectable.** Boot-time init was the
   only thing the "self-healing" claims covered: `cameraOk_` was never
   unset after boot, and the RC522 `healthy_` flag was never re-probed. Now
   3 consecutive `fb_get` failures flip the camera into the existing
   re-init cadence (`CAMERA_FAILURES_BEFORE_REINIT`), and the RC522 probes
   VersionReg every `RC522_REINIT_INTERVAL_MS` while healthy — a chip that
   dies mid-day drops into the re-init branch instead of being silently deaf.

5. **Small reliability + hygiene:** PSRAM-less boards fall back to a DRAM
   capture buffer; the buzzer chirp uses elapsed-style (wrap-safe) timing;
   `extractLongField` tolerates whitespace after the colon and avoids the
   `isdigit(char)` UB; the boot placeholder warning also flags
   `API_BASE_URL` set to `localhost`/`127.0.0.1` (the templates now ship a
   LAN placeholder); dead `Mode::callType()` / `modeKindToString()` removed;
   station banner lines completed bilingually; `secrets.camera.h.example`
   no longer claims the station has no mode console.

## Consequences

- Native host tests: 93 → 96 (feedLine routing precedence, non-command
  routing, no-residue). `pio run` succeeds for `esp32dev` and `esp32cam`.
- Docs updated where they were wrong: HARDWARE_SETUP (EN/ES) now shows the
  READER pin map (SCK 18 / MISO 19 / MOSI 23 / SS 5 / RST 27) — it had
  presented the STATION's pins as the reader defaults — plus the real
  library names/versions; test counts corrected to 96.
