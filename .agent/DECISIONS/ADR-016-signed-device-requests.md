# ADR-016: signed device requests (Pulse-HMAC)

## Status
Accepted (2026-09-16, TASK-043)

## Context
B2B-Core's red-team audit (RT-001) showed every device secret crossing
the competition hotspot in cleartext HTTP (`Authorization: Bearer
<api_key>` per tap), so one sniffed packet buys full reader
impersonation. TLS was rejected for the demo path (certificate
provisioning per reader outweighs the threat on a physically
controlled LAN, and it endangers the no-hardcoded-IP discovery flow).
B2B-Core answers with ADR-062: per-request HMAC with nonce-only
freshness (no timestamps — the ESP32 has no wall clock).

## Decision
1. **New `PresenceCore/RequestSigner` (pure, host-tested): the ONLY
   place the Authorization VALUE is built.** `kid =
   sha256hex(secret)[0:16]`, `sig =
   HMAC-SHA256(secret, "METHOD\npath\nnonce\nsha256hex(body)")`,
   header `Pulse-HMAC kid:nonce:sig`. No new secret to provision: the
   kid derives from the existing `READER_API_KEY`, which becomes a
   signing key that never leaves the device.
2. **Crypto is not duplicated**: `RequestSigner` builds on the HCE
   file's compact SHA-256/HMAC (`Hce::sha256Bytes` newly exposed next
   to the existing `hmacSha256`) — one SHA-256 copy in the repo.
3. **One HTTP choke point signs everything**: `EspApiClient::post`
   (both images route through it — `postToBackend`, `Station::post`)
   mints a fresh 128-bit `esp_random` nonce per request and signs the
   EXACT bytes POSTed (JSON and multipart alike — the hash is
   length-delimited, NUL-safe for JPEG bodies). Constructor signature
   keeps its shape `(baseUrl, key, timeout)`; the key just changed
   meaning from "sent secret" to "signing secret".
4. **Discovery/reconnect untouched**: signing is per-request and
   IP-agnostic — mDNS re-pointing, DHCP churn, and reboot recovery
   work exactly as before. The failed POST is still never retried;
   classroom retries now collapse server-side (B2B-Core first-tap
   dedup), so the no-retry rule no longer risks double attendance.
5. **Interop is pinned, not hoped**: `test_request_signer.cpp` asserts
   the same golden literals B2B-Core's `DeviceHmacAuthTest` asserts
   (body hash, kid, signature, full header shape). Either side drifting
   fails its own build before any flash.

## Consequences
- Captured hotspot traffic holds single-use signatures: replays 401
  (server nonce cache), retargets fail (body-bound signature).
- 401 keeps its existing meaning and remediation (`printReaderKeyRemediation`
  now also covers kid mismatch after rotation — reflash the new key).
- `bearerAuthorizationValue` (TASK-007) stays pinned for the bench
  Bearer path; devices no longer send it.
- Build identity bump (`hce.16` → `hce.17`, bench rule).
