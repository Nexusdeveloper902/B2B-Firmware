# STATE SNAPSHOT — after RUN-2026-09-05-firmware-002

## Repository state

- Branch: main at the TASK-002 merge commit (feature/
  TASK-002-rc522-default merged --no-ff; see `git log` for the hash)
- Working tree: clean
- b2b-core: untouched by this run (its main stays at e1350db+)

## What the firmware does now (delta vs RUN-001)

- **Default build = real RC522 reader** (ADR-004): plain `pio run` and
  `pio run -t upload` build/flash the RC522/SPI firmware. The mock
  env is explicit opt-in: `pio run -e esp32dev-mock`.
- Boot banner is impl-specific: the real build invites "present a card
  / presenta una tarjeta"; only the mock build invites typed UIDs.
- Rc522NfcReader hardened: explicit SPI pins from config.h
  (SCK 18 / MISO 19 / MOSI 23 / SS 5 / RST 27, all configurable),
  self-health tracking with non-blocking re-init every 5 s on reader
  loss (no reboot), bilingual diagnostics on the injected Print stream:
  `[NFC] RC522 detected — firmware version 0x.. / detectado` (positive
  radio confirmation) or `[NFC] RC522 NOT responding (version 0x..) —
  check wiring (…pins…) + power 3.3 V` (self-explaining failure).
- Everything from RUN-001 unchanged: two modes, full backend loop,
  48/48 native tests, bilingual docs, E2E harness.

## Verification status

| Item | Status |
|---|---|
| esp32dev (real RC522) compile | PASS (RAM 14.4% / Flash 72.1%) |
| esp32dev-mock compile | PASS |
| plain `pio run` selects real env | PASS |
| native unit tests | 48/48 PASS |
| Physical flash + RC522 radio reads | PENDING — user bench (checklist §1) |
| Reader self-recovery on real hardware | PENDING — user bench (unplug/replug reader) |

## Confirmed facts

- User's bench: ESP32 + RC522 present, backend reachable at
  http://192.168.1.6:8000, device got IP 192.168.1.7 (from the boot log
  the user pasted — the mock build was running, Wi-Fi + backend path
  are proven working end-to-end by that session).
- The RC522 driver is real (miguelbalboa/rfid): PCD_Init + VersionReg
  probe + PICC_IsNewCardPresent/PICC_ReadCardSerial + HaltA +
  StopCrypto1 — no stubs anywhere in the tap path.

## Next steps for whoever picks this up

1. User: `pio run -t upload` + `pio device monitor` — expect
   `Reader impl / Implementacion: RC522 (SPI)` and the
   `[NFC] RC522 detected ...` line; then walk
   docs/MANUAL_VERIFICATION_CHECKLIST.md (§1 wiring confirmation).
2. If wiring differs from the defaults, edit include/config.h (all
   five RC522 pins + the re-init interval live there).
3. Optional deferred work (untouched, non-goals): OTA, captive portal,
   recycling classification follow-up, remote mode reconfiguration.
