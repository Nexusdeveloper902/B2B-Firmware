# API Integration — what the firmware calls and expects

> También disponible en: [Español](API_INTEGRATION.es.md)
> Authoritative source: the [B2B-Core](https://github.com/Nexusdeveloper902/B2B-Core)
> repository — `routes/api.php` + controllers (this file mirrors the
> verified contract, verified 2026-09-05 against B2B-Core `main`).

All calls are plain, stateless, versioned HTTP — the backend's Hardware
Abstraction Principle: anything that can make an authenticated HTTP POST
works identically, whether Postman, curl, a test, or this firmware.

## Common conventions

- **Base URL**: discovered at boot via DNS-SD (`_pulse._tcp.local`,
  advertised by B2B-Core `./run serve` — `lib/PulseDiscovery`, policy in
  `PresenceCore/PulseEndpoint.h`, pinned by `test_pulse_endpoint.cpp`).
  `API_BASE_URL` in `include/secrets.h` (e.g. `http://192.168.1.50:8000`
  — no trailing slash) is only the boot fallback when discovery finds
  nothing. Any transport failure re-queries (cooldown-guarded) and
  re-points the client, so a DHCP-rotated backend is picked up with no
  reboot or reflash; the failed POST itself is never retried (taps are
  not idempotent).
- **Auth**: `Authorization: Pulse-HMAC <kid>:<nonce>:<sig>` (ADR-016).
  Every POST is signed with `READER_API_KEY` and a fresh `esp_random`
  nonce (HMAC-SHA256 over method, path, nonce and body hash; image posts
  sign the multipart canonical `event_id + image.sha256`, not the raw
  bytes — PHP never sees raw multipart) — the key
  itself NEVER rides the wire, so a hotspot capture holds one single-use
  signature: replays 401, retargeted bodies fail. The backend never
  trusts a client-supplied reader id. The key is printed by the B2B-Core
  DemoSeeder (`./run setup`).
  The firmware builds this header **explicitly** — the literal scheme
  comes from `Presence::Signer::authorizationValue()` (PresenceCore,
  pinned by `test_request_signer.cpp` against B2B-Core's golden vector),
  and `EspApiClient` sends it via `addHeader`. Never switch back to
  `HTTPClient::setAuthorization(key)`: that method prefixes its
  **default authorization type `Basic`**, the backend ignores
  `Authorization: Basic <key>` entirely (TASK-007 history), and the
  legacy `Bearer <key>` bench path must never return to devices.
- **Content-Type**: `application/json` for JSON posts; the camera station
  posts images as `multipart/form-data` via `EspApiClient::postMultipart`
  (signed over the multipart canonical — see above; wire bytes in
  `CapturePayload`, canonical pinned by `test_capture_payload.cpp`).
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

- `credential_uid` — the scanned credential: uppercase hex UID string for
  RC522 physical cards (any string in mock mode), or the
  application-level credential id for an HCE phone tap (e.g.
  `TEST-ANDROID-001` — never the phone's RF UID, which Android
  randomizes per tap; see [HCE_PROTOCOL.md](HCE_PROTOCOL.md)). Required.
- `hce_nonce` / `hce_mac` — HCE phone taps only (TASK-015, ADR-018): the
  reader's 8-byte CHALLENGE nonce (16 hex) and the phone's HMAC (64 hex),
  relayed unverified — B2B-Core checks them with that credential's own
  key and answers `403` when they do not verify (parsed as
  `CardNotRecognized`). Omitted for physical cards. The same two fields
  ride on `/recycling/captures/{id}/associate`.
- `client_timestamp` — optional ISO 8601 device clock; the firmware
  currently omits it and lets the server timestamp the event (a broken
  device clock must never lose the tap — the backend also degrades
  gracefully).

**Responses and firmware handling** (parsed by `Presence::parseTapResponse`):

| HTTP | Backend meaning | Parsed outcome | Firmware feedback |
|---|---|---|---|
| 200 | `{ "status": "ok", "event_id": 1042, "event_type": "CLASS_ATTENDANCE", "student_first_name": "Maria", "next_step": null }` | `TapOutcome::Success` | EVENT LED solid 1.5 s (+ serial log with student + type; `next_step == "awaiting_classification"` is logged, nothing more — classification is out of scope for the reader (the ESP32-CAM station auto-captures+classifies on it — see CAMERA_STATION.md) |
| 401 | bad signature / unknown kid / replayed nonce → `{ "status": "error", "message": "..." }` | `TapOutcome::AuthFailure` | 6 fast blinks |
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

HCE phones add the capture kind AND the one-time key hand-off (built by
`Presence::buildPairPayload(uid, reader.lastKind(), reader.lastProof())`
— `"hce"` only when `poll()` completed SELECT AID + CHALLENGE + ENROLL):

```json
{
  "credential_uid": "PLS-K3Y7V3CT0R5Z",
  "credential_kind": "hce",
  "hce_nonce": "0123456789abcdef",
  "hce_mac": "ba6d0fbf…a295",
  "hce_key_nonce": "000102030405060708090a0b0c0d0e0f",
  "hce_key_wrapped": "c5bb9fe1…14c2"
}
```

`hce_key_wrapped` is the phone's key XOR a pad derived from
`READER_API_KEY` (never the raw key). The backend refuses an `hce` pair
without these fields (`422`), answers `403` when the proof does not
verify under the handed-over key (`PairOutcome::ProofRejected`, session
stays armed), and re-keys an active phone of the same armed student
(`"rekeyed": true`). The kind is stored as `cards.kind`; omitting it
pairs as `physical`, so physical-card bodies are unchanged. Full
byte-level spec: [HCE_PROTOCOL.md](HCE_PROTOCOL.md).

**Responses and firmware handling** (parsed by `Presence::parsePairResponse`):

| HTTP | Backend meaning | Parsed outcome | Firmware feedback |
|---|---|---|---|
| 200 | `{ "status": "ok", "paired_student_name": "Maria González", "student_id": 3 }` | `PairOutcome::Success` | EVENT LED solid 1.5 s; serial log names the student |
| 401 | bad signature / unknown kid / replayed nonce | `PairOutcome::AuthFailure` | 6 fast blinks |
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
