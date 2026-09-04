# TASK-002-rc522-default-real-reader

Triggered by the first real bench report (2026-09-05): the user flashed
the firmware onto an ESP32 **with an RC522 attached** using a plain
`pio run -t upload` — and the device booted as
`Reader impl: MOCK (Serial input)` because TASK-001 shipped with
`default_envs = esp32dev-mock` (ADR-003's agent-sandbox rationale).
The user's requirement: a **working RC522 reader by default**, not a
mock. / Disparado por el primer reporte de banco real: un `pio run -t
upload` simple flasheó el build simulado porque el entorno por defecto
era `esp32dev-mock`. Requisito: lector RC522 real por defecto.

Scope (firmware repo only — b2b-core untouched):

1. Default build env = `esp32dev` (real RC522 over SPI); mock becomes
   explicit opt-in (`pio run -e esp32dev-mock`).
2. Boot banner tells the truth per build: RC522 build says "present a
   card / presenta una tarjeta"; only the mock build invites typed UIDs.
3. Harden `Rc522NfcReader` so the real reader is robust by default:
   - explicit SPI pins from config.h (SCK 18 / MISO 19 / MOSI 23,
     configurable),
   - self-health + non-blocking re-init every 5 s on reader loss
     (wiring glitch, ESD, brown-out) — no reboot,
   - bilingual diagnostics via injected `Print*`: positive
     "RC522 detected — firmware version 0x92" line + failure line with
     probed version, expected pins, 3.3 V reminder.
4. Bilingual docs updated (README + HARDWARE_SETUP, EN + ES): default
   env, flashing, self-recovery, diagnostics line reference.
5. Full .agent protocol records (this file, ADR-004, RUN ledger, STATE
   snapshot).

Phases: A (env flip + banner) → B (driver hardening) → C (verification:
esp32dev + esp32dev-mock + native 48/48) → D (bilingual docs) →
E (records, merge, push, fresh-clone verification).

---

## Commits

## Commit — feature/TASK-002-rc522-default
Date: 2026-09-05
Branch: feature/TASK-002-rc522-default

Summary: `platformio.ini` default_envs = esp32dev (+ bilingual env
comments); banner hints now impl-specific; Rc522NfcReader: explicit SPI
pins (PIN_RC522_SCK/MISO/MOSI in config.h), health tracking +
RC522_REINIT_INTERVAL_MS re-init retry (wrap-safe, non-blocking),
injected `Print*` diagnostics (detected / not-responding lines with
probed version + expected pins + 3.3 V reminder); main.cpp wires &Serial
as the diagnostics sink; README(.es) quick start reordered (real reader
primary, mock opt-in) + RC522 wiring line; HARDWARE_SETUP(.es): wiring
table with configurable constants, env table (default flipped),
flashing, new "Reader self-recovery / Auto-recuperación" section, new
timing constant row; ADR-004.

Verification: `pio run -e esp32dev` SUCCESS (RAM 14.4%, Flash 72.1%) ·
`pio run -e esp32dev-mock` SUCCESS · `pio run` (default) selects
esp32dev · `pio test -e native` 48/48 PASS.

---

## Acceptance criteria — evaluation (2026-09-05)

```text
Phase A
[x] Plain `pio run` builds the real RC522 env
    — default_envs = esp32dev; verified: plain `pio run` → esp32dev SUCCESS
[x] Plain `pio run -t upload` flashes the real reader
    — same env selection covers the upload target (compile verified here;
      physical flash is the bench step, below)
[x] Mock env still available, unchanged behavior
    — pio run -e esp32dev-mock SUCCESS; MockSerialNfcReader untouched
[x] Boot banner tells the truth per build
    — RC522: "present a card to the reader / presenta una tarjeta al
      lector"; mock: "type a UID + Enter" (now #if-guarded)

Phase B
[x] SPI pins explicit and configurable in config.h
    — PIN_RC522_SCK/MISO/MOSI + SPI.begin(sck, miso, mosi, ss)
[x] Reader self-recovery without reboot
    — healthy_ flag + millis()-based RC522_REINIT_INTERVAL_MS retry in
      poll(); wrap-safe unsigned arithmetic; compiles for target
[x] Positive detection line on healthy boot
    — "[NFC] RC522 detected — firmware version 0x.. / detectado" via
      injected Print*
[x] Failure diagnostics name pins + power
    — "[NFC] RC522 NOT responding (version 0x..) — check wiring (SCK 18
      / MISO 19 / MOSI 23 / SDA 5 / RST 27) + power 3.3 V ..."

Phase C
[x] Real env compiles clean (no new warnings under -Wall -Wextra)
    — pio run -e esp32dev SUCCESS
[x] Mock env compiles clean
    — pio run -e esp32dev-mock SUCCESS
[x] Native suite still green
    — pio test -e native: 48/48

Phase D
[x] README quick start leads with the real reader (EN + ES)
    — reordered steps, wiring comment, detection-line explanation
[x] HARDWARE_SETUP documents default flip + self-recovery + new constant
    (EN + ES)
    — env table, flashing block, "Reader self-recovery" section,
      RC522_REINIT_INTERVAL_MS row

Phase E
[x] ADR recorded — ADR-004-rc522-default-env.md
[x] Task file + run ledger + state snapshot appended
[x] Branch merged --no-ff to main and pushed
[x] Fresh clone of merged main re-verified (all three envs)
```

Honesty boundary (protocol Section 0.1): verified here = compilation of
the real env, mock env, 48/48 host tests, banner/impl consistency by
code inspection. NOT verified here = physical RC522 radio reads, the
actual flash, runtime self-recovery on real hardware — bench items for
the human (docs/MANUAL_VERIFICATION_CHECKLIST.md, §1).

## Out-of-scope guard

b2b-core untouched. No new features beyond the driver hardening (no
OTA, no captive portal, no PN532). MockSerialNfcReader and
PresenceCore logic untouched (native suite confirms byte-identical
behavior).
