# TASK-003-serial-mode-console

User requirement (2026-09-05): "Change the mode changing behaviour — it
should now work so you input a password on terminal and it switches
modes (it'll be stored in the secrets file ofc)."

Replaces the boot-time mode button (ADR-002, superseded) with a
runtime serial-console password toggle (ADR-005). Firmware repo only —
b2b-core untouched.

Phases: A (secrets/config/console core) → B (wiring + feedback) →
C (verification) → D (bilingual docs) → E (records, merge, push).

---

## Commits

## Commit — feature/TASK-003-serial-mode-console
Date: 2026-09-05
Branch: feature/TASK-003-serial-mode-console

Summary: lib/PresenceCore ModeConsole.h/.cpp (LineBuffer + password
state machine, injected time, wrap-safe lockout); MODE_PASSWORD in
secrets.h.example + legacy #ifndef fallback with bilingual #warning in
main.cpp; config.h: mode-console knobs, button pins deleted (GPIO 32
freed); main.cpp: single Serial owner (masked `*` echo), line dispatch,
runtime mode toggle with [MODE] logs, no boot button read;
MockSerialNfcReader refactored to pushLine (no Serial ownership);
FeedbackKind + patterns: ModeSwitched (2x500ms) / ModeRejected (2x80ms);
15 new native tests (test_console.cpp); docs EN+ES updated (README,
HARDWARE_SETUP incl. new console section + BOM, CHECKLIST §1/§4 with
password steps incl. lockout case, API_INTEGRATION trigger wording);
ADR-005; ADR-002 marked superseded; PROJECT.md mode paragraph updated.

Verification: `pio run -e esp32dev` SUCCESS · `pio run -e
esp32dev-mock` SUCCESS · `pio test -e native` 63/63 PASS (48 prior +
15 new).

---

## Acceptance criteria — evaluation (2026-09-05)

```text
Phase A
[x] Mode password stored in the gitignored secrets.h
    — MODE_PASSWORD in secrets.h.example; #ifndef fallback + #warning
      keeps a legacy secrets.h compiling after pull
[x] Console logic is pure C++ and host-testable
    — lib/PresenceCore/src/ModeConsole.h/.cpp, injected uint32 time
[x] Line assembly is robust (CRLF, trim, overflow, bare Enter)
    — LineBuffer + 5 dedicated tests

Phase B
[x] Typing the correct password toggles the mode at runtime
    — main.cpp dispatchConsoleLine/switchMode; [MODE] switched to ...;
      2 slow EVENT blinks; MODE LED shows the new idle immediately
[x] Wrong passwords are counted and lock the console
    — 3 wrongs → 10 s lockout, even correct refused, then reset
      (MODE_CONSOLE_MAX_WRONG_ATTEMPTS / _LOCKOUT_MS, wrap-safe)
[x] Typed characters never appear on screen
    — masked '*' echo in pollConsole; log never prints the password
[x] Mode-switch outcomes have distinct LED patterns
    — ModeSwitched (2x500 ms) / ModeRejected (2x80 ms); patterns test
[x] Boot behavior: OPERATION, no button
    — mode = &operationMode; selectModeFromPin + PIN_MODE_SELECT/
      MODE_LEVEL_PAIRING/MODE_BUTTON_SETTLE_MS deleted; GPIO 32 free
[x] Mock build still supports typed-UID taps
    — non-password lines → MockSerialNfcReader::pushLine (reader no
      longer owns Serial); documented that lockout is real-build only

Phase C
[x] Real env compiles clean
    — pio run -e esp32dev SUCCESS
[x] Mock env compiles clean
    — pio run -e esp32dev-mock SUCCESS
[x] Native suite green, console covered
    — 63/63: match/accepted/rejected/lockout/expiry/wrap/overflow/
      trim/CRLF/patterns-distinct

Phase D
[x] README EN+ES: mode table + password switching + secrets list
[x] HARDWARE_SETUP EN+ES: BOM (button gone), console section, patterns,
    timing constants, banner sample, flashing comment
[x] CHECKLIST EN+ES: §1.1 boot line, §4 rewritten as password steps
    incl. wrong-password lockout case (4.4) and switch-back (4.5)
[x] API_INTEGRATION EN+ES: mode trigger wording updated
[x] ADR-005 recorded; ADR-002 marked superseded; PROJECT.md updated

Phase E
[x] Task file + run ledger + state snapshot
[x] Branch merged --no-ff to main and pushed
[x] Fresh clone of merged main re-verified (all three envs)
```

Honesty boundary (protocol Section 0.1): verified here = compilation
of both ESP32 envs, 63/63 host tests for the console state machine,
banner/impl consistency by code inspection. NOT verified here = typing
into a real Serial Monitor, LED patterns on hardware, lockout timing
on-device — bench items for the human (checklist §1.1, §4.1–4.5).

## Out-of-scope guard

b2b-core untouched (main stays @ e1350db+). No persisted mode across
reboots (NVS), no remote mode reconfiguration, no second factor, no
hardware button revival — all deferred/non-goals (ADR-005
consequences).
