# ADR-015: backend discovery via DNS-SD inside the existing net split

## Status
Accepted (2026-09-15, TASK-013)

## Context
Every image dialed a compile-time `API_BASE_URL`, so each DHCP rotation
of the backend machine meant editing secrets, recompiling and reflashing
every device. The brief asked for `_pulse._tcp.local` discovery with a
`network/ WiFiManager / PulseDiscovery / PulseConnection / PulseWebSocket`
shape. The repo already has its own split — `WifiService` (association)
+ `EspApiClient` (POST) + host-tested `PresenceCore` — and no WebSocket
client anywhere (the `:8081` realtime feed is dashboard-browsers only;
the station's `WebServer:80` is the on-device visualizer, not a client).

## Decision
1. **New `lib/PulseDiscovery` (Arduino-only, ESPmDNS from the core —
   no new dependency): the ONLY file that touches mDNS.** `begin()`
   starts the responder once Wi-Fi associates; `discover()` runs one
   blocking `queryService("pulse","tcp")` round (~3 s in-core, so boot
   and failure paths only — never per tap, never on a timer);
   `refreshIfDue()` cooldown-guards the failure path (no multicast
   storms). No `PulseConnection`/`PulseWebSocket` classes: with no WS
   client in the firmware they would be speculation (YAGNI).
2. **Selection policy in `PresenceCore/PulseEndpoint.h` (pure,
   host-tested):** first answer with usable IPv4 + non-zero port +
   compatible `protocol` TXT wins; `protocol` present-but-!=`1` rejects
   (future wire gate), absent TXT accepts (minimal advertisements work).
   `pulse.local` is never assumed — the device dials the resolved IP.
3. **One HTTP choke point per image** (`postToBackend` in `main.cpp`,
   `Station::post` in the station): any transport failure re-queries and
   re-points `EspApiClient` via `setBaseUrl()`; the failed POST is NEVER
   retried (taps are not idempotent — a timeout may follow a commit).
   Next tap uses the fresh endpoint: rotation recovery with no reboot.
4. **Compiled `API_BASE_URL` stays as boot fallback** (never blank-URL),
   so a missing advertisement degrades to the old behavior instead of
   bricking. No NVS endpoint cache: the repo has no NVS system today and
   the brief forbids inventing persistent config for this — discovery on
   failure already covers rotation.
5. **Build identity bumped** (`hce.15` → `hce.16`, bench rule).

## Consequences
Native 118/118 (6 new); `esp32dev`, `esp32dev-mock`, `esp32cam-reader`,
`esp32cam` all SUCCESS. Bench rotation test + no-advertisement fallback
stay human-verified (checklist in B2B-Core `docs/MDNS.md`). Residual:
link-local mDNS only — a routed venue net needs real DNS/a reflector.
