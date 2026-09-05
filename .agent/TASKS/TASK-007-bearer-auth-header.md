# TASK-007-bearer-auth-header

Owner's bench report (2026-09-05): pairing armed correctly via the new
dashboard desk (student 3, 45 s window), card 62041607 tapped in PAIRING
mode — `[401] reader key rejected`, twice, even though the key had been
proven working earlier via the TASK-005 verification curl. The backend's
serve log shows the device's two `POST /api/v1/admin/cards/pair` calls
arriving (timestamps match the taps) and both rejected at
`ResolveReaderToken`.

Diagnosis: not a key problem at all. The ESP32 Arduino `HTTPClient`
prefixes `setAuthorization(key)` values with its DEFAULT authorization
type `"Basic"` — the device has been sending `Authorization: Basic <key>`
since TASK-001, and Laravel only reads the `Bearer ` scheme, so EVERY
real-hardware call ever made answered 401. curl verification (header
spelled out manually) and native tests (mocked transport) could not see
it. B2B-Core is verified correct and untouched by this task.

## Deliverables

1. `Presence::bearerAuthorizationValue(key)` — inline helper in
   PresenceCore `CoreTypes.h` (next to the `ApiCall` request contract):
   returns exactly `"Bearer " + key`. The scheme literal lives in
   host-testable code, not in a transport library default.
2. `EspApiClient.h` — sends
   `addHeader("Authorization", bearerAuthorizationValue(key))`;
   `setAuthorization()` is never called (its built-in auth block stays
   empty → single Authorization header on the wire). Header + inline
   comments document the trap bilingually.
3. `test/test_auth.cpp` — 3 native regression tests (exact prefix,
   32-char key unchanged, empty key → bare scheme), registered as
   `runAuthTests()` in `test_main.cpp` → 68/68.
4. Docs (EN/ES parity):
   - API_INTEGRATION.md + .es.md — the Auth convention bullet now
     explains how the header is built and forbids
     `setAuthorization(key)` (with the historical 401-always bug).
   - PAIRING.md + .es.md — §2 (the 401 checklist) opens with "be on
     TASK-007 or later" (older firmware 401s with a VALID key);
     Troubleshooting's `[401]` entry says update-first, then provision.
5. `.agent` records: ADR-007, this task file, RUN-2026-09-05-firmware-007,
   STATE snapshot, PROJECT.md facts.

## Acceptance

- [x] `bearerAuthorizationValue` in PresenceCore, pinned by native tests
- [x] EspApiClient no longer calls `setAuthorization`; sends the
      explicit `Authorization: Bearer <key>` header
- [x] `pio test -e native` 68/68 (65 + 3 new)
- [x] `pio run -e esp32dev` + `-e esp32dev-mock` SUCCESS
- [x] `strings firmware.elf` shows the `Bearer ` literal in both builds
- [x] Docs EN/ES updated (API_INTEGRATION + PAIRING)
- [x] Secret scan of tracked files clean
- [x] No protocol change: paths, payloads, status mapping, serial lines,
      remediation text untouched

## Out of scope (deliberately)

- No change to main.cpp remediation lines (a 401 on fixed firmware again
  genuinely means provisioning; the existing text is accurate).
- No backend changes (B2B-Core verified correct; its own TASK-012 fixes
  the phone/LAN session issue reported in the same bench session).
- `GET /api/v1/reader/me` boot-time key check — still deferred (ADR-007
  consequences).
