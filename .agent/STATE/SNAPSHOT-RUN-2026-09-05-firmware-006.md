# STATE SNAPSHOT — after RUN-2026-09-05-firmware-006

## Repository state

- Branch: main at the TASK-006 merge commit (feature/
  TASK-006-dashboard-arming-pointer merged --no-ff; see `git log` for
  the hash)
- Working tree: clean
- b2b-core: updated by ITS task this same day — TASK-011 pairing desk,
  main @ e242028 (verified there: 155 tests, quality, e2e 22/22)

## What the firmware does now (delta vs RUN-005)

No behavior change — docs-only:

- docs/PAIRING.md + .es.md teach the dashboard pairing desk as the
  RECOMMENDED arming path (log in as admin → "Pair cards" /
  «Emparejar tarjetas» → "Arm pairing" click — no PAT, no curl, live
  countdown + success line + history), with the former curl + PAT
  instructions kept verbatim as the automation alternative.
- CHECKLIST EN/ES §5.1 arms via the page first.
- The arm-then-pair protocol, mode console, hints, remediation lines,
  and all code are exactly as TASK-003/004/005 left them.

## Confirmed facts (cumulative, still current)

- Default build = real RC522 (ADR-004); mock is explicit opt-in
- Mode switching = serial password (ADR-005); boots OPERATION
- Pairing = arm-then-pair (B2B-Core ADR-020/021); arming is now a
  dashboard one-click (B2B-Core TASK-011); the device pairs with its
  Bearer reader key exactly as before
- 401 = key not provisioned; 404 on the verification curl = key
  ACCEPTED (TASK-005 finding)

## Bench status (owner, live backend 192.168.1.6:8000)

- Wi-Fi + RC522 + serial password mode switching: WORKING
- Reader key: PROVEN WORKING (TASK-005 evidence)
- New shortest path to first successful pairing: `./run serve` on the
  backend → log in as admin → nav **Pair cards** → **Arm pairing** on
  the student's row → type MODE_PASSWORD on the device → tap card
  62041607 → success line on BOTH the page and the serial monitor.

## Next steps for whoever picks this up

1. Owner bench: one end-to-end pairing through the dashboard page +
   the device (checklist §5 as now written).
2. If the desk UI ever feels off (polling cadence, history length):
   knobs are recorded in B2B-Core TASK-011 (2 s poll, 8-row history).
3. Possible follow-ups (NOT started): backend GET /reader/me key
   check; deterministic seeded UIDs.
