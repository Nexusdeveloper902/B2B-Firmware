# STATE SNAPSHOT — after RUN-2026-09-05-firmware-007

## Repository state

- Branch: main at the TASK-007 merge commit (feature/TASK-007-bearer-
  auth-header merged --no-ff; see `git log` for the hash)
- Working tree: clean
- b2b-core: updated by ITS task this same day — TASK-012 stateful LAN
  access fix (phone dashboard use), see its records

## What the firmware does now (delta vs RUN-006)

- The Authorization header is built by the firmware itself:
  `Presence::bearerAuthorizationValue(key)` (PresenceCore,
  host-test-pinned) → `EspApiClient` sends
  `Authorization: Bearer <READER_API_KEY>` via addHeader.
  `HTTPClient::setAuthorization()` is never called — its default
  authorization type is "Basic", which the backend ignores.
- Everything else is unchanged: endpoints, payloads, status mapping,
  mode console, hints, remediation lines, LED patterns.

## Confirmed facts (cumulative, still current)

- Default build = real RC522 (ADR-004); mock is explicit opt-in
- Mode switching = serial password (ADR-005); boots OPERATION
- Pairing = arm-then-pair (B2B-Core ADR-020/021); arming is a dashboard
  one-click (B2B-Core TASK-011); pairing desk usable from LAN phones
  after B2B-Core TASK-012
- **Real-hardware transport auth now sends the correct Bearer scheme
  (TASK-007/ADR-007)** — before 2026-09-05 the device sent
  `Authorization: Basic <key>` and every call 401'd with a valid key
- A 401 on current firmware = key genuinely not provisioned
  (PAIRING.md §2); the 404-on-verification-curl = key-check PASS
  finding (TASK-005) still holds

## Bench status (owner, live backend 192.168.1.6:8000)

- Wi-Fi + RC522 + serial password mode switching: WORKING
- Reader key: backend row proven (TASK-005 curl); DEVICE transport
  proven correct only from TASK-007 on — the owner's persistent 401s
  were the Basic/Bearer transport bug, NOT a bad key
- Shortest path to first successful pairing: backend `git pull` +
  restart serve → firmware `git pull` + `pio run -e esp32dev -t upload`
  → log in as admin (desktop or phone) → **Pair cards** → **Arm
  pairing** → type MODE_PASSWORD on the device → tap card 62041607 →
  `[OK] card paired to: <student>` on serial + success line on the page
- Earlier 401'd taps consumed NOTHING (401 precedes pairing logic);
  card 62041607 is still fresh and unpaired

## Open items

- `GET /api/v1/reader/me` boot-time key check: still deferred
  (nice-to-have; transport is now correct)
- First real end-to-end pairing on hardware is the owner's bench item
  (agent verified builds, tests, binary literal — not live hardware)
