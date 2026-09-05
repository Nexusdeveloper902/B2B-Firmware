# TASK-004-pairing-mode-guidance

User report (2026-09-05, bench log): after switching to PAIRING mode with
the serial password and tapping a card, the serial log showed only
`[401] reader key rejected / clave de lector rechazada`. User feedback:
"That's not how pairing is supposed to work — also, please document
pairing mode more."

Investigation (main agent, this run):
- The firmware pairing path is CORRECT: PAIRING mode POSTs
  `/api/v1/admin/cards/pair` with `Authorization: Bearer READER_API_KEY`
  (byte-identical to the verified TASK-010 contract). No code defect.
- The 401 comes from B2B-Core `ResolveReaderToken`: no `readers` row
  matches the secrets.h `READER_API_KEY` (the DemoSeeder generates and
  prints its own random keys at `./run setup`; a user-invented key was
  never provisioned server-side).
- Separately, pairing is arm-then-pair (B2B-Core ADR-020): an admin must
  POST `/api/v1/admin/students/{id}/arm-pairing` (45 s window) BEFORE the
  card tap. Nothing was armed, so even a registered key would have
  produced `[409] No pairing session active`.
- Root UX defect: the device never teaches its own flow. Mode switch
  prints no operator guidance; 401/409/422 print no remediation; no
  document walks the end-to-end pairing procedure. This task fixes the
  operator experience and the documentation — NOT the (correct) protocol.

Firmware repo only — b2b-core untouched (its flow is correct and was
verified 20/20 live in RUN-2026-09-03-firmware-001).

Phases: A (Mode::hint core + tests) → B (serial remediation lines) →
C (verification: both builds + native suite) → D (bilingual docs, new
docs/PAIRING.md + .es.md + cross-links) → E (records, merge, push).

---

## Acceptance criteria — evaluation (2026-09-05)

```text
Phase A
[x] Mode strategy exposes a bilingual operator hint (pure C++, testable)
    — Mode::hint() in lib/PresenceCore/src/Mode.h; implemented in Modes.h/.cpp
[x] Pairing hint teaches: arm first (45 s window), tap a FRESH card,
    points at docs/PAIRING.md
    — PairingMode::hint(); the 45 s mirrors the backend default (commented)
[x] Native tests cover the hint content (bilingual, actionable, distinct)
    — test_modes.cpp: test_pairing_hint_teaches_arm_first_flow,
      test_mode_hints_bilingual_distinct_nonempty

Phase B
[x] Mode switch prints the hint after the [MODE] switched line
    — main.cpp switchMode() prints "[MODE] " + mode->hint()
[x] 401 (both modes) prints remediation: key has no reader row on the
    backend → docs/PAIRING.md provisioning
    — printReaderKeyRemediation() called from both Tap and Pair branches
[x] 409 prints: arm a session first, then tap — docs/PAIRING.md
[x] 422 prints: use a FRESH card (session stays armed)
[x] 404 (operation) prints: unpaired card? pair it (PAIRING mode)

Phase C
[x] pio run -e esp32dev SUCCESS
[x] pio run -e esp32dev-mock SUCCESS
[x] pio test -e native 65/65 PASS (63 prior + 2 new)

Phase D
[x] docs/PAIRING.md + docs/PAIRING.es.md: what pairing IS/IS NOT, two-step
    design + rationale, prerequisites (reader-key provisioning with exact
    commands), full walkthrough with serial lines, outcome table, LED
    patterns, troubleshooting, FAQ, security notes
[x] README EN+ES: PAIRING.md in doc table + mode row link + stale native
    test count fixed (48 → 65)
[x] API_INTEGRATION EN+ES: pairing section cross-links PAIRING.md
[x] CHECKLIST EN+ES: §4.3/§5.1 reference PAIRING.md; expected serial lines
    updated for the new remediation lines (also §setup, §3.1, §3.3, §4.1,
    §6.2)
[x] secrets.h.example: READER_API_KEY comment states the 401 failure mode
[x] PROJECT.md: modes paragraph points at docs/PAIRING.md

Phase E
[x] ADR-006 recorded
[x] Task file + run ledger + state snapshot
[x] Branch merged --no-ff to main and pushed
[x] Fresh clone of merged main re-verified
```

Honesty boundary (protocol Section 0.1): verified here = compilation of
both ESP32 envs, 65/65 host tests (incl. hint content), doc facts
cross-checked against B2B-Core source read this run (routes,
ResolveReaderToken, PairingService, DemoSeeder, presence config). NOT
verified here = the new serial lines on real hardware and a live
end-to-end pairing — bench items (checklist §3.1/§3.3/§4.1/§4.3/§6.2 +
docs/PAIRING.md walkthrough).

## Out-of-scope guard

b2b-core untouched (main stays @ e1350db+). No new backend endpoint (a
reader key-check endpoint `GET /reader/me` is noted as a possible
follow-up, NOT built). No change to the arm-then-pair protocol, the 45 s
window, or the mode-switch password mechanism (ADR-005 stands).
