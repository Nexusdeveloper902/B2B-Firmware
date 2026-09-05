# API Integration — what the firmware calls and expects

> También disponible en: [Español](API_INTEGRATION.es.md)
> Authoritative source: the [B2B-Core](https://github.com/Nexusdeveloper902/B2B-Core)
> repository — `routes/api.php` + controllers (this file mirrors the
> verified contract, verified 2026-09-05 against B2B-Core `main`).

All calls are plain, stateless, versioned HTTP — the backend's Hardware
Abstraction Principle: anything that can make an authenticated HTTP POST
works identically, whether Postman, curl, a test, or this firmware.

## Common conventions

- **Base URL**: from `API_BASE_URL` in `include/secrets.h` (e.g.
  `http://192.168.1.50:8000` — no trailing slash).
- **Auth**: `Authorization: Bearer <READER_API_KEY>`. The key IS the
  reader identity; the backend never trusts a client-supplied reader id.
  The key is printed by the B2B-Core DemoSeeder (`./run setup`).
  TASK-007: the firmware builds this header **explicitly** — the literal
  `Bearer ` prefix comes from `Presence::bearerAuthorizationValue()`
  (PresenceCore, pinned by `test_auth.cpp`), and `EspApiClient` sends it
  via `addHeader`. Never switch back to `HTTPClient::setAuthorization(key)`:
  that method prefixes its **default authorization type `Basic`**, the
  backend ignores `Authorization: Basic <key>` entirely, and every
  real-hardware call answered 401 with a perfectly valid key before this
  fix (curl verification never caught it — curl sends the header
  verbatim).
- **Content-Type**: `application/json` (the classify endpoint alone uses
  multipart — not used by this firmware; see "Out of scope").
- **Localization**: error `message` text is localized by the backend via
  `Accept-Language`. The firmware therefore NEVER branches on message
  text — it decides on the HTTP status code and the `status` field, and
  treats the message as display-only serial-log content.
- **Timeouts**: the firmware bounds every request (`HTTP_TIMEOUT_MS`,
  default 10 s); transport failures map to the network-failure feedback
  pattern and the device stays responsive.

## OPERATION MODE — POST /api/v1/events/tap

Triggered by a card tap while the device is in OPERATION mode (the boot
default; switch modes with the serial-console password — TASK-003).

**Request** (built by `Presence::buildTapPayload`):

```json
{
  "credential_uid": "A1B2C3D4",
  "client_timestamp": "2026-09-05T07:58:00-05:00"
}
```

- `credential_uid` — the scanned UID (uppercase hex string for RC522
  UIDs; any string in mock mode). Required.
- `client_timestamp` — optional ISO 8601 device clock; the firmware
  currently omits it and lets the server timestamp the event (a broken
  device clock must never lose the tap — the backend also degrades
  gracefully).

**Responses and firmware handling** (parsed by `Presence::parseTapResponse`):

| HTTP | Backend meaning | Parsed outcome | Firmware feedback |
|---|---|---|---|
| 200 | `{ "status": "ok", "event_id": 1042, "event_type": "CLASS_ATTENDANCE", "student_first_name": "Maria", "next_step": null }` | `TapOutcome::Success` | EVENT LED solid 1.5 s (+ serial log with student + type; `next_step == "awaiting_classification"` is logged, nothing more — classification is out of scope) |
| 401 | missing/invalid Bearer key → `{ "status": "error", "message": "..." }` | `TapOutcome::AuthFailure` | 6 fast blinks |
| 404 | unknown card (`Card not recognized`) or inactive card (`Card is not active`) | `TapOutcome::CardNotRecognized` | 2 blinks; message logged |
| 422 | validation error | `TapOutcome::ValidationError` | long solid (server-error style) |
| 5xx | unexpected server error | `TapOutcome::ServerError` | long solid |
| transport | timeout / DNS / connection refused | `TapOutcome::NetworkError` | 5 fast blinks |

Any unrecognized combination parses to `UnknownError` → long solid
pattern; the loop continues normally (the device never wedges on a
malformed response). Since TASK-004, the 401/404 tap lines are followed
by bilingual remediation hints (key provisioning / pair-the-card
pointers to PAIRING.md).

## PAIRING MODE — POST /api/v1/admin/cards/pair

Triggered by a card tap while the device is in PAIRING mode (entered by
typing the serial-console mode password — TASK-003).
The endpoint was built in B2B-Core as `TASK-010-card-pairing-endpoint`
(two-step arm-then-pair design; arm window default 45 s — see B2B-Core
ADR-020). The complete operator guide — why arming comes first,
reader-key provisioning, arming how-to with an admin PAT, per-outcome
troubleshooting, FAQ — is [PAIRING.md](PAIRING.md) (TASK-004).

**Request** (built by `Presence::buildPairPayload`):

```json
{ "credential_uid": "A1B2C3D4" }
```

**Responses and firmware handling** (parsed by `Presence::parsePairResponse`):

| HTTP | Backend meaning | Parsed outcome | Firmware feedback |
|---|---|---|---|
| 200 | `{ "status": "ok", "paired_student_name": "Maria González", "student_id": 3 }` | `PairOutcome::Success` | EVENT LED solid 1.5 s; serial log names the student |
| 401 | missing/invalid Bearer key | `PairOutcome::AuthFailure` | 6 fast blinks |
| 409 | no active pairing session → `{ "status": "error", "message": "No pairing session active" }` | `PairOutcome::NoActiveSession` | 3 blinks; message logged |
| 422 | card already paired (or malformed uid) → `{ "status": "error", "message": "Card already paired" }` | `PairOutcome::AlreadyPaired` | 4 blinks; message logged |
| 5xx / other | unexpected | `ServerError` / `UnknownError` | long solid |
| transport | timeout / DNS / refused | `PairOutcome::NetworkError` | 5 fast blinks |

### Arming a pairing session (backend side, NOT done by firmware)

Pairing succeeds only while a pending pairing session is armed for a
student. Arm it from an admin session or personal access token (how to
mint an admin PAT: [PAIRING.md](PAIRING.md) §Prerequisites):

```bash
# against the running B2B-Core backend
curl -X POST http://<backend>/api/v1/admin/students/<id>/arm-pairing \
     -H "Authorization: Bearer <admin-PAT-or-session>" \
     -H "Accept: application/json"
# → { "status": "ok", "student_id": <id>, "expires_at": "..." }  (45 s window)
```

Then tap the fresh card on the reader in PAIRING MODE within the window.

## Out of scope for this firmware (deliberately)

- `POST /api/v1/recycling/classify` (camera/classification flow) — the
  tap response's `next_step: "awaiting_classification"` is logged only.
- `POST /api/v1/admin/readers/{id}/mode` (remote reader relabeling).
- Redemption, NL-query, and all dashboard-only endpoints.
