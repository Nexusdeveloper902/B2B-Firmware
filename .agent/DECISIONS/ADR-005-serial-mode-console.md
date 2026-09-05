# ADR-005: Mode switching via serial-console password (supersedes ADR-002)

## Status
Accepted (2026-09-05, TASK-003)

## Context
ADR-002 selected the operating mode at boot from a physical button
(GPIO 32, held during power-on → PAIRING). Two years of bench reality
aside, the actual driver was the user's explicit TASK-003 requirement:
"I don't want to hold a button at power-on — I want to type a password
on the terminal and have it switch modes, stored in the secrets file."

Additional problems with the button design that this removes:
- Entering PAIRING required a power cycle (bad for a device mounted in
  an enclosure).
- Anyone with physical access could enter PAIRING mode with no gate at
  all (though pairing itself was still backend-gated).

## Decision
1. **Runtime toggle, not boot selection.** The device boots in OPERATION
   mode. Typing the correct mode password + Enter in the Serial Monitor
   toggles OPERATION <-> PAIRING at any time, no reboot. Mode is session
   state (not persisted across reboots).
2. **Password value in the gitignored secrets.h** (`MODE_PASSWORD`,
   added to secrets.h.example). Tunable knobs in config.h:
   `MODE_CONSOLE_MAX_WRONG_ATTEMPTS` (3), `MODE_CONSOLE_LOCKOUT_MS`
   (10000), `SERIAL_LINE_MAX_LENGTH` (64). A legacy secrets.h without
   MODE_PASSWORD still compiles via an `#ifndef` fallback + bilingual
   `#warning` (same pattern as TASK-002's buildability fix).
3. **New PresenceCore component** `ModeConsole` (+ `LineBuffer`): pure
   C++ with injected time (CardDebouncer pattern), fully host-testable.
   Wrong-attempt counting, 10 s lockout after 3 consecutive wrongs
   (wrap-safe elapsed-time math), counter reset on success/expiry.
4. **Serial ownership moves to main.cpp.** The composition root reads
   Serial once (masked `*` echo — typed secrets never appear on
   screen), assembles lines, and dispatches:
   - Real-reader build: every non-empty line is a password attempt.
   - Mock build: a line matching the password toggles the mode;
     anything else is forwarded to MockSerialNfcReader.pushLine() as a
     virtual card tap (the mock reader no longer owns Serial). The
     lockout is therefore a real-build behavior — counting dev UIDs as
     wrong passwords would break the tap workflow.
5. **Feedback vocabulary grows**: `ModeSwitched` (2 slow 500 ms blinks)
   and `ModeRejected` (2 very fast 80 ms blinks) join FeedbackKind; the
   continuous MODE LED remains the always-visible source of truth for
   the current mode. The log never prints the expected password.
6. **The mode button is removed**: PIN_MODE_SELECT /
   MODE_LEVEL_PAIRING / MODE_BUTTON_SETTLE_MS deleted from config.h;
   GPIO 32 freed; docs/BOM/checklist updated. ADR-002 is superseded.

## Rationale
- The Serial Monitor is inherently a physical-access (USB) channel, so
  a password there is an operator convenience gate, not cryptography —
  and that is honest about what it protects: accidental mode changes
  and casual tampering. The real security boundary for pairing stays
  server-side (admin-armed 45 s session + reader Bearer key), exactly
  as designed in B2B-Core TASK-010 / ADR-020.
- Runtime switching removes the power-cycle ritual and keeps both
  modes one password away for the operator.
- Keeping the console logic in PresenceCore (not main.cpp) preserves
  the "hardware-independent logic is host-testable" discipline; 15 new
  native tests cover match/lockout/expiry/wrap/line-assembly.

## Consequences
- The bench operator needs the Serial Monitor attached to switch modes
  (documented in the checklist; a future hardware button or remote
  reconfiguration could return as a separate ADR if wanted).
- A user's existing secrets.h keeps compiling but should gain
  MODE_PASSWORD (the compiler warns until then).
- Mock-build UX change: typed lines are checked against the password
  first; a UID equal to the password would toggle the mode instead of
  tapping (pathological — pick different values).
