# STATE SNAPSHOT — after RUN-2026-09-03-firmware-001

## Repository state

- Branch: main at the TASK-001 merge commit (feature/
  TASK-001-reader-firmware-mvp merged --no-ff; see `git log` for the hash)
- Genesis commit d467771 (phases A–D, F, G-firmware) landed directly on
  main as the repo's first buildable state; the E1-dependent tail (Phase
  E2 harness + closure docs) came through the feature branch
- Working tree: clean; scripts/e2e_backend.sh carries the executable bit

## What the firmware can do now

- Compile for the real target (`pio run -e esp32dev`, RC522/SPI) and the
  mock target (`esp32dev-mock`, serial-typed virtual taps — default env)
- Two modes selected at boot from GPIO32: OPERATION (tap → event) and
  PAIRING (tap → card linked to the armed student) — continuous mode LED
- Full backend loop over real HTTP, Bearer reader key: tap + pair, every
  documented outcome with a distinct LED pattern + bilingual serial log
- 48/48 host unit tests; 8/8 real-backend E2E verdicts via
  scripts/e2e_backend.sh (firmware payloads + parser against a live
  B2B-Core)

## Phase-by-phase status

| Phase | Status |
|---|---|
| A — scaffolding/config | DONE (builds; secrets discipline held) |
| B — NFC abstraction | DONE (RC522 + mock behind one interface) |
| C — mode system | DONE (boot-time pin, continuous LED, strategies) |
| D — operation mode | DONE (live-backend verified) |
| E1 — b2b-core TASK-010 | DONE (in that repo: merged 667e4dd+, verified, pushed) |
| E2 — pairing firmware | DONE (live-backend verified: 8/8) |
| F — resilience/feedback | DONE (non-blocking, debounce, FeedbackController) |
| G — tests/docs | DONE (48 host tests; bilingual real docs; bench checklist) |

Pending: hardware-in-the-loop verification (human bench, 27-item
checklist) — RC522 radio, Wi-Fi association, LED/button physics.

## Confirmed facts

- NFC chip: RC522 over SPI (settled; abstraction still swappable — ADR-003)
- Pairing endpoint dependency: RESOLVED — b2b-core main carries
  POST /api/v1/admin/cards/pair (TASK-010, ADR-020, 45 s window)
- Library reality: miguelbalboa/MFRC522 @ ^1.4.11 (registry), ArduinoJson ^7.4.3

## Next steps for whoever picks this up

1. Bench-run the manual verification checklist (EN + ES) with the board.
2. Optional deferred work (explicit non-goals of TASK-001, untouched):
   recycling classification follow-up, remote mode reconfiguration, OTA,
   captive-portal provisioning.
3. b2b-core side: dashboard "Pair new card" button follow-up lives in
   THAT repo's task file, not here.
