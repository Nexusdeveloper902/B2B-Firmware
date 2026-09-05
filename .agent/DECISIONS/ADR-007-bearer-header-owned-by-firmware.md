# ADR-007: The Authorization header value is firmware-owned (Bearer), never HTTPClient's default

## Status
Accepted (2026-09-05, TASK-007)

## Context
Owner's bench (2026-09-05, live backend 192.168.1.6:8000, arming done via
the TASK-011 dashboard desk, student 3, fresh card 62041607):

```text
[NFC] card / tarjeta: 62041607
[401] reader key rejected / clave de lector rechazada
```

The arming POST and the status polling all worked; the two
`/api/v1/admin/cards/pair` POSTs from the device show up in the backend's
`php artisan serve` log at the exact tap moments — and both answered 401.

Root cause (read from the arduino-esp32 HTTPClient source, framework
3.20017): `HTTPClient::setAuthorization(const char*)` stores the value in
`_base64Authorization` and the request builder emits

```text
Authorization: <_authorizationType> <value>      // _authorizationType defaults to "Basic"
```

So `EspApiClient`'s `http.setAuthorization(READER_API_KEY)` sent
`Authorization: Basic <key>`. Laravel's `bearerToken()` only extracts
tokens after the literal `Bearer ` prefix; `ResolveReaderToken` therefore
saw an EMPTY token and returned 401 **with a perfectly valid,
correctly provisioned key**.

Every prior verification path missed it:
- curl examples in the docs spell out `-H "Authorization: Bearer ..."`
  verbatim — they never exercised the firmware transport;
- backend feature tests forge the HTTP request directly;
- native firmware tests mock the ApiClient seam;
- the owner's earlier "404 Card not recognized" bench evidence proved the
  key check passed *for the curl*, not for the device.
The device had, in fact, never once successfully authenticated.

An earlier state-snapshot line — "Reader key: PROVEN WORKING (TASK-005
evidence)" — was over-broad: the evidence proved the backend-side key
row, not the device transport. This ADR corrects that.

## Decision
1. **The Authorization header VALUE is built by the firmware, in
   testable code.** `Presence::bearerAuthorizationValue(key)` (an inline
   helper in PresenceCore's `CoreTypes.h`, next to the `ApiCall` request
   contract) returns exactly `"Bearer " + key`.
2. **`EspApiClient` sends it via `addHeader("Authorization", value)`.**
   `setAuthorization()` is never called, so HTTPClient's built-in auth
   block stays empty and this is the single Authorization header on the
   wire. The literal scheme can no longer be decided by a transport
   library default.
3. **A native regression test pins the scheme**
   (`test/test_auth.cpp`, registered in `test_main.cpp`): exact
   `"Bearer "` prefix, a full 32-char key passes through unchanged, and
   an empty key yields the bare scheme (backend answers 401 — same
   remediation path as a bad key).
4. **Docs record the trap** (API_INTEGRATION EN/ES conventions bullet +
   PAIRING EN/ES §401 checklist and troubleshooting): any 401 seen on
   firmware older than TASK-007 is this bug — update first; on current
   firmware a persistent 401 genuinely means key provisioning.

## Rationale
- The bug class is "HTTP detail decided by a library default instead of
  the contract". The contract (`Authorization: Bearer <key>`,
  B2B-Core ADR-002) must live where tests can see it; PresenceCore is
  host-testable, HTTPClient is not.
- `addHeader` over `setAuthorizationType("Bearer") + setAuthorization`:
  the scheme string then lives entirely in testable code (not split
  between a library call argument and the value), and the approach is
  independent of HTTPClient API drift across framework versions.
- No protocol change: path, payload, status mapping, serial remediation
  lines all stay exactly as TASK-004/005 documented them. Only the header
  bytes on the wire change — from wrong to right.

## Consequences
- Real-hardware taps can authenticate for the first time; the pairing
  flow should now complete end-to-end (bench item for the owner).
- `strings firmware.elf | grep -c "^Bearer $"` returns 1 in both
  environments — the literal is verifiably in the binaries.
- The 401 remediation text in main.cpp ("no matching reader row")
  becomes TRUE in the strict sense only from this task on; docs say so.
- Follow-up still open (deferred again): backend `GET /api/v1/reader/me`
  boot-time key check — now less urgent since the transport is fixed.
