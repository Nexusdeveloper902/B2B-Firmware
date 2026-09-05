# ADR-006: Pairing-mode operator guidance (the device teaches its own flow)

## Status
Accepted (2026-09-05, TASK-004)

## Context
A bench run of TASK-003's serial password switch produced:

```text
[MODE] switched to / cambiado a: PAIRING / EMPAREJAR
[NFC] card / tarjeta: 62041607
[401] reader key rejected / clave de lector rechazada
```

User report: "That's not how pairing is supposed to work — also, please
document pairing mode more."

Investigation found NO protocol defect: the firmware had issued the
correct TASK-010 call (`POST /api/v1/admin/cards/pair`, Bearer key), and
the 401 was the backend correctly reporting that no `readers` row matches
the secrets.h key. Two operator-invisible facts caused the confusion:

1. The reader key must be provisioned **server-side** (the DemoSeeder
   prints its own random keys; a user-invented secrets.h key is unknown
   to the backend).
2. Pairing is **arm-then-pair** (B2B-Core ADR-020): an admin must arm a
   45 s session before any tap can pair a card.

The device communicated neither — a mode switch with no guidance, and a
bare `[401]` with no path to recovery. The operator's only screen is the
Serial Monitor.

## Decision
1. **Do not change the protocol.** Arm-then-pair, the 45 s window, the
   one-shot session, the fresh-card rule and server-side key provisioning
   all stand (B2B-Core ADR-020). The defect was local operator guidance,
   and that is what gets fixed.
2. **`Mode::hint()`** joins the strategy interface (with `label()`):
   a bilingual, possibly multi-line operator guidance string printed by
   `switchMode()` right after the `[MODE] switched to` line. It lives in
   PresenceCore (pure C++), so it is host-testable — two new native tests
   pin its content (pairing hint must mention arming, the 45 s window,
   fresh cards, and docs/PAIRING.md; both hints non-empty, bilingual and
   distinct).
3. **Remediation lines for every actionable failure**, printed after the
   status line (main.cpp, both mode branches):
   - 401 (tap + pair): the key has no reader row on the backend →
     docs/PAIRING.md §provisioning.
   - 409: arm a session first, then tap within the window.
   - 422: use a FRESH card; the session stays armed (a real backend
     behavior — a 422 does not consume the pending pairing).
   - 404 (tap): unpaired card? switch to PAIRING and arm a session.
   The backend's localized `message` keeps being printed as-is; the
   firmware still never branches on message text.
4. **`docs/PAIRING.md` + `docs/PAIRING.es.md`** become the canonical
   pairing guide: what the mode IS/IS NOT, the arm-then-pair rationale,
   prerequisites (three key-provisioning options + a verification curl),
   a full happy-path walkthrough with exact serial lines, the outcome
   table, LED patterns, troubleshooting, FAQ and security notes. READMEs,
   API_INTEGRATION, CHECKLIST and secrets.h.example cross-link it.
5. **The "45 s" in the hint mirrors the backend default**
   (`presence.pairing_window_seconds`); it is display-only guidance, not
   a contract. The doc says the backend is always authoritative.

## Rationale
- The Serial Monitor is the operator's only screen; a device that can
  401 without saying how to fix it forces the operator to read backend
  source. Guidance at the moment of failure is the cheapest fix that
  respects the security model (nothing is weakened — the console still
  cannot provision keys, arm sessions or reassign cards).
- Putting the hint in the strategies (not main.cpp) preserves the
  "hardware-independent logic is host-testable" discipline; the hint
  content is pinned by tests so bilingual parity cannot silently rot.
- The 422 remediation encodes a real, previously undocumented backend
  behavior (session survives a 422) that lets an operator retry
  immediately instead of re-arming.

## Consequences
- Mode-switch and failure log output is now 2–4 lines longer; the
  checklist's expected serial lines were updated accordingly.
- Docs must stay truthful about backend config drift (window size); the
  hint text is pinned by tests, so changing the default requires a
  conscious edit here too.
- Possible follow-up (NOT built): a backend `GET /api/v1/reader/me`
  key-check endpoint so the device could validate its key at boot or at
  pairing entry instead of waiting for a tap to 401. Deferred — the
  remediation lines + docs already close the loop.
