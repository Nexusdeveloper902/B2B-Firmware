# TASK-006-dashboard-arming-pointer

Owner's request (2026-09-05, right after the pairing-desk task was
scoped): "make a change (you decide where, maybe in the core repo but
it's up to you), same protocol — I don't really want to make an
individual manual post request each time I wanna pair some new
student." The functional fix landed in B2B-Core as TASK-011 (the
pairing desk, ADR-021 — `/admin/pairing`, one-click arming under the
admin session; merged to its main @ e242028 and verified there:
155 tests, quality gate, e2e 22/22, fresh-clone).

This repo's share of the work: the canonical pairing guide
(docs/PAIRING.md + .es.md, built in TASK-004) still teaches
curl + admin-PAT as THE way to arm. Point it at the pairing desk as
the recommended path so the two repos tell one story. Docs-only.

## Acceptance criteria — evaluation (2026-09-05)

```text
[x] PAIRING.md TL;DR: STEP 1 now names the dashboard "Pair cards"
    page + Arm pairing click (curl kept as the automation alternative)
[x] PAIRING.md Prerequisites 4 retitled "A way to arm — dashboard page
    (recommended) or admin token": Option 1 = pairing desk (no PAT, no
    curl, live countdown + success line + history), Option 2 = the
    former PAT instructions, kept verbatim for scripts
[x] PAIRING.md walkthrough step 1: dashboard variant first, curl as the
    automation alternative
[x] PAIRING.md FAQ "Who can arm": mentions the TASK-011 one-click page
[x] PAIRING.es.md: full mirror of all four changes
[x] MANUAL_VERIFICATION_CHECKLIST.md + .es.md §5.1: dashboard arming
    recommended + curl alternative kept
[x] .agent records: this task file, RUN-2026-09-05-firmware-006, STATE
    snapshot, PROJECT.md fact
[x] Regression guard: pio run -e esp32dev SUCCESS · esp32dev-mock
    SUCCESS · pio test -e native 65/65 (docs-only change; binary
    content unchanged)
[x] Branch merged --no-ff to main and pushed; fresh clone re-verified
```

## Out-of-scope guard

- No firmware code change (mode strategies, hints, remediation lines,
  serial behavior all stand as TASK-003/004 left them).
- The 45 s window, arm-then-pair protocol, and secrets discipline
  untouched.
- The pairing desk itself, its status endpoint, and the card_id audit
  column live in B2B-Core TASK-011/ADR-021 — referenced here, never
  duplicated.
