# STATE SNAPSHOT — after RUN-2026-09-05-firmware-003

## Repository state

- Branch: main at the TASK-003 merge commit (feature/
  TASK-003-serial-mode-console merged --no-ff; see `git log`)
- Working tree: clean
- b2b-core: untouched by this run (its main stays at e1350db+)

## What the firmware does now (delta vs RUN-002)

- **Mode switching is a serial-console password** (ADR-005; supersedes
  ADR-002): the device boots in OPERATION and the operator toggles
  OPERATION <-> PAIRING at runtime by typing `MODE_PASSWORD`
  (gitignored secrets.h; example template updated) + Enter in the
  Serial Monitor. Typed chars echo as `*`; `[MODE]` log lines report
  switch/reject/lockout; 3 wrong passwords lock the console 10 s.
- The boot-time mode button is REMOVED: no wiring, GPIO 32 free,
  BOM/checklist updated.
- Mock build: non-password lines are virtual card taps (mock reader is
  now a pushLine consumer — main.cpp owns Serial exclusively).
- New host-tested core: lib/PresenceCore ModeConsole + LineBuffer
  (pure C++, injected time, wrap-safe). 63/63 native tests.
- New LED patterns: ModeSwitched (2 slow blinks) / ModeRejected
  (2 very fast blinks).
- Everything from RUN-001/002 unchanged: RC522 default build, reader
  self-recovery, two modes' backend contracts, bilingual docs.

## Verification status

| Item | Status |
|---|---|
| esp32dev (real RC522) compile | PASS |
| esp32dev-mock compile | PASS |
| native unit tests | 63/63 PASS (15 new console tests) |
| Physical Serial typing / lockout timing / new LED patterns | PENDING — bench (checklist §4.1–4.5) |

## Confirmed facts

- User's bench workflow now includes the Serial Monitor for mode
  switching (they were already using the terminal — this matches how
  they actually operate the device).
- Legacy secrets.h compatibility: builds with a bilingual #warning
  until MODE_PASSWORD is added (no hard break on `git pull`).
- ADR-002 (boot-time pin) is superseded; the pin constants no longer
  exist in config.h.

## Next steps for whoever picks this up

1. User: add `#define MODE_PASSWORD "..."` to include/secrets.h, `pio
   run -t upload`, then walk checklist §1 + §4 (password switch,
   wrong-password lockout) with the real board.
2. Possible follow-ups (NOT started): persist mode across reboots
   (NVS), remote mode reconfiguration, an additional hardware button.
