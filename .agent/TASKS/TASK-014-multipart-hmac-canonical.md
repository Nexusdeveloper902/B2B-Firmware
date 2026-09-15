# TASK-014 — multipart Pulse-HMAC canonical (station classify 401 fix)

## Ask
Field report (station `hce.17`): HCE tap authenticates, tap logs
`RECYCLING_DEPOSIT` with `next_step: awaiting_classification`, then
card-first `POST /api/v1/recycling/classify` answers `401 Invalid
device signature` on every retry while the arm is kept.

## Root cause
Both sides signed "the body" but meant different bytes: this firmware
signed the exact multipart wire bytes (ADR-016 §3), while B2B-Core
hashed `$request->getContent()` — ALWAYS empty for multipart
(PHP never exposes raw `multipart/form-data` via `php://input`).
Mismatch on every image post; JSON taps unaffected. Uncaught because
no test on either side ever signed a multipart post.

## Firmware (this repo) — DONE
- `PresenceCore/CapturePayload`: new `classifySigningBody()` /
  `captureSigningBody()` — the signing canonical both sides sign
  (classify: `"event_id=<id>\nimage.sha256=<hex>"`, capture:
  `"image.sha256=<hex>"`), byte-exact mirror of B2B-Core
  `DeviceRequestSigner::multipartCanonical()` (ADR-017).
- `EspApiClient`: new `postMultipart(path, body, contentType,
  signingBody)` (shared `doPost` core); `post()` keeps signing the
  exact bytes (JSON). `Station::postMultipart()` choke point with the
  same rediscovery rule; classify + capture use it, tap/associate
  untouched.
- Build `hce.17` → `hce.18` (bench rule). Docs: `API_INTEGRATION.md`
  (+`.es.md`) auth + Content-Type bullets.
- Tests: `test_capture_payload.cpp` +3 (both canonicals pinned to the
  shared `sha256('abc')` literal, image/event binding + NUL safety).

## Backend (B2B-Core TASK-044) — DONE (separate repo)
- `DeviceRequestSigner::multipartCanonical()` + `canonicalBody()`;
  `verify()` reconstructs the canonical from the parsed upload.
  Contract: ADR-063 (amends ADR-062).

## Out of scope (recorded, not forgotten)
- `hce.17` stations keep 401ing until reflashed — the kept arm makes
  it visible, not silent; no OTA path exists, bench flash only.
- Reader envs (`esp32dev*`) never send multipart; untouched except via
  the shared client (builds re-proven).
