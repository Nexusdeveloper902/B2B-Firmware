# STATE SNAPSHOT — after RUN-2026-09-05-firmware-004

## Repository state

- Branch: main at the TASK-004 merge commit (feature/
  TASK-004-pairing-mode-guidance merged --no-ff; see `git log`)
- Working tree: clean
- b2b-core: untouched by this run (its main stays at e1350db+)

## What the firmware does now (delta vs RUN-003)

- **The device teaches its own flow** (ADR-006): every mode switch
  prints a bilingual `[MODE]` hint (pairing: arm a session first, 45 s
  window, FRESH card, docs/PAIRING.md; operation: tap a PAIRED card).
- **Every actionable failure prints remediation** after the status line:
  401 → reader-key provisioning pointer (both modes); 409 → arm-first
  lines; 422 → fresh-card + session-stays-armed lines; 404 (tap) →
  pair-the-card pointer.
- **docs/PAIRING.md + .es.md** — the canonical pairing guide: IS/IS-NOT,
  arm-then-pair rationale, prerequisites (3 key-provisioning options,
  verification curl, admin PAT minting), happy-path walkthrough, outcome
  table, LED patterns, troubleshooting, FAQ, security notes.
- Mode strategy interface grew `hint()`; 2 new native tests pin its
  content (65/65 total).
- Everything from RUN-001/002/003 unchanged: RC522 default build, reader
  self-recovery, serial-password mode switching (ADR-005), both modes'
  backend contracts, bilingual docs.

## Verification status

| Item | Status |
|---|---|
| esp32dev (real RC522) compile | PASS |
| esp32dev-mock compile | PASS |
| native unit tests | 65/65 PASS (2 new hint tests) |
| New serial hint/remediation lines on hardware | PENDING — bench (checklist §3.1, §3.3, §4.1, §4.3, §6.2) |
| Live end-to-end pairing (arm → tap → [OK]) | PENDING — bench (docs/PAIRING.md walkthrough) |

## Confirmed facts

- The pairing protocol (arm-then-pair, 45 s window, one-shot, fresh-card
  rule, server-side key provisioning) is unchanged and correct — the
  user's `[401]` was an unprovisioned key plus no armed session, now
  covered by guidance + docs.
- Re-running `./run setup` on B2B-Core re-prints the SAME reader keys
  (firstOrCreate; additive setup) — the documented Option A for key
  provisioning.
- A 422 does NOT consume the armed session (PairingService); the
  remediation line states this so operators retry immediately.

## Next steps for whoever picks this up

1. User: provision the reader key (docs/PAIRING.md §Prerequisites —
   Option A copy the seeder key, or Option B pin your own), reflash,
   then walk the PAIRING.md happy path (arm → tap fresh card → [OK]).
2. Possible follow-ups (NOT started): backend `GET /api/v1/reader/me`
   key-check endpoint for proactive key validation; dashboard button for
   arming pairings (today it is an API call).
