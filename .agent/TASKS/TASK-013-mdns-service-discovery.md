# TASK-013 — mDNS service discovery (`_pulse._tcp.local`), no hard-coded backend IP

## Date opened
2026-09-15

## Origin
Owner: remove the hard-coded backend IP from the ESP32 firmware; devices
must discover the Pulse backend dynamically (DHCP/IP rotation) via
mDNS/DNS-SD service `_pulse._tcp.local`, with timeouts/retries/logging
and recovery without reboot, reflash or config edits.

## Investigation (before any edit)
- The hard-coded IP lives in exactly one value: `API_BASE_URL`
  (`include/secrets.h` / `secrets.camera.h`), consumed in exactly two
  places — `src/main.cpp` (`static EspApiClient api(…)`) and
  `src/camera/station.{h,cpp}` (`api_(…)` concatenates `baseUrl_ + path`).
- Backend ports: API/web `8000` (`B2B_SERVE_PORT`), realtime WS `8081` —
  dashboard browsers ONLY. The firmware has no WebSocket client at all
  (nothing to migrate; the station's `WebServer:80` is the on-device
  visualizer). So the advertised port is always the API port.
- Reconnect today: `WifiService` (15 s bounded boot + 10 s background
  re-tick) + 10 s HTTP timeout → `NetworkError` feedback, next tap
  retries the SAME dead URL. No backend rediscovery.
- No mDNS library, no NVS/Preferences anywhere in the firmware.
  PlatformIO + espressif32 + Arduino; `ESPmDNS` ships in the installed
  core (verified against
  `~/.platformio/packages/framework-arduinoespressif32/…/ESPmDNS`:
  `queryService(service,proto)` ~3 s in-core, `IP(i)`/`port(i)`/
  `hasTxt`/`txt`) — no new dependency.
- Suitable seam exists: `WifiService` + `EspApiClient` + host-tested
  `PresenceCore`. The brief's `PulseConnection`/`PulseWebSocket` names
  were adapted, not copied (no WS client → no WS class).

## Delivered
- `lib/PresenceCore/src/PulseEndpoint.h` (pure): service identity
  defines, `PulseCandidate`/`PulseEndpoint` (`baseUrl()`/`urlFor()`/
  `fromBaseUrl()` fallback parser), `pulseProtocolCompatible()` gate,
  `pickPulseEndpoint()` selection — all host-tested.
- `lib/PulseDiscovery/src/PulseDiscovery.h` (Arduino-only, the single
  mDNS touchpoint): `begin()` / `discover()` / `refreshIfDue()` /
  `lastSeen()`. `include/config/common.h`: `PULSE_DISCOVERY_BOOT_ATTEMPTS
  2`, `PULSE_REDISCOVER_COOLDOWN_MS 15000`.
- `EspApiClient`: `setBaseUrl()` + `baseUrl()` (runtime re-pointing).
- `src/main.cpp`: boot discovery (bounded, then compiled fallback),
  `postToBackend()` choke point (failure → cooldown-guarded rediscovery,
  failed POST never retried), late-Wi-Fi `begin()`, banner prints the
  effective URL. `src/camera/station.{h,cpp}`: same via
  `discoverPulseServer()` + `Station::post()` covering tap, capture,
  classify and associate through one guard.
- `API_BASE_URL` demoted to boot fallback in both secrets templates +
  `docs/API_INTEGRATION.md` (+`.es.md`); build `hce.15` → `hce.16`.
- Backend half is B2B-Core TASK-043 (serve.sh advertisement, Avahi file,
  `docs/MDNS.md` + `.es.md`, ADR-061); contract shared byte-for-byte.

## Deliberately skipped (not forgotten)
- NVS endpoint cache: no NVS system exists in this firmware and the
  brief says not to invent persistent config — discovery-on-failure
  already covers rotation.
- `pulse.local` hard-coding: service discovery (IP + port) is
  authoritative; the hostname is informational (see B2B-Core ADR-061).
- WebSocket migration: no WS client exists in the firmware.

## Verification
- `pio test -e native` 118/118 (112 before + 6 new
  `test_pulse_endpoint.cpp`).
- `pio run -e esp32dev` / `-e esp32dev-mock` / `-e esp32cam-reader` /
  `-e esp32cam` all SUCCESS.
- Pending (human bench): rotation test + no-advertisement fallback —
  procedure in B2B-Core `docs/MDNS.md`.
