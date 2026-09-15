# ADR-017: signed multipart canonical (image-post 401 fix)

## Status
Accepted (2026-09-15, TASK-014)

## Context
ADR-016 §3 said "sign the EXACT bytes POSTed (JSON and multipart
alike)". That is unimplementable for image posts: B2B-Core can never
see raw multipart bytes (`php://input` is empty for
`multipart/form-data`), so it hashed `""` while the device hashed the
real body. Field-proven: station `hce.17` taps fine, every
classify/capture 401s. B2B-Core answers with ADR-063: image posts sign
a canonical both sides can reconstruct.

## Decision
1. **Image posts sign the canonical, not the wire bytes**:
   `CapturePayload::classifySigningBody()` /
   `captureSigningBody()` build `"event_id=<id>\nimage.sha256=<hex>"`
   / `"image.sha256=<hex>"` over the raw image bytes only (never the
   framing) — byte-exact mirror of B2B-Core
   `DeviceRequestSigner::multipartCanonical()`, pinned by shared
   literals in `test_capture_payload.cpp`.
2. **One choke point per kind**: `EspApiClient::postMultipart()`
   (same transport contract as `post()`, shared `doPost` core) takes
   the wire body AND the signing body; `Station::postMultipart()`
   carries the same rediscovery rule as `Station::post()`. JSON posts
   keep signing exact bytes via `post()` — tap/associate diff-free.
3. **Crypto still not duplicated**: the canonical hashes through the
   existing `Signer::sha256Hex` (binary-safe, NUL-safe); no new
   crypto, no new secret, no provisioning change, discovery/reconnect
   untouched.
4. **Build bump** (`hce.17` → `hce.18`, bench rule) so serial logs
   self-identify the fixed signer.

## Consequences
- `hce.17` and earlier classify/capture signatures will NEVER verify
  (by design — they signed bytes the backend cannot see); reflash to
  `hce.18` is the only fix, bench flash only.
- Replays still 401 (nonce cache), retargets still fail (image-bound
  canonical), captures stay single-use.
- `API_INTEGRATION.md` (+`.es.md`) auth + Content-Type bullets now
  state the canonical; the reader-env "out of scope" note stands
  (readers never send multipart).

## Amends
ADR-016 §Decision-3 (multipart signing input)
