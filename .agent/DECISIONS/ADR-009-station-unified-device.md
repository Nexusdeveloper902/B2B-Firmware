# ADR-009: ESP32-CAM station as one integrated device

## Date
2026-09-07

## Context
The `esp32cam` env was a camera-only firmware while the validated
hardware is a complete station (camera + RC522 aboard, bench-proven
2026-09-07: DIAG-CAM VersionReg 0x92 stable + raw SPI agree on
SS 13 / SCK 14 / MOSI 15 / MISO 2 / RST 4). Two firmware halves could
not share one transaction (tap → classify), and two headers
(`config.h` / `config_camera.h`) had already diverged (RST 16 vs 4,
buzzer 4 vs -1).

## Decision
1. `src/camera/station.h/.cpp` owns ONE `Station` lifecycle
   (`begin()`/`update()`); `src/camera/main.cpp` only delegates.
2. Hardware config is `include/config/{common,esp32dev,esp32cam}.h`
   with `include/config.h` dispatching on `-DCAMERA_STATION`
   (esp32cam env). `config_camera.h` deleted.
3. A tap answered `awaiting_classification` + event id
   auto-captures+classifies via the existing `classifyWithEvent` path.
   No new backend protocol.
4. Failures are recoverable states (camera re-init 30 s, NFC 5 s,
   Wi-Fi via WifiService); no FATAL halts. One status LED (GPIO33,
   active-LOW) + `StationDegraded`/`CaptureSuccess` feedback kinds.
5. `secrets.h` / `secrets.camera.h` stay separate backend identities.

## Alternatives Considered
- Copying `src/main.cpp` into the camera app: rejected — duplicates
  the tap pipeline and the two copies would diverge.
- Merging the two secrets files: rejected — different backend
  identities by design.
- New RFID→photo backend endpoint: rejected — `classifyWithEvent`
  already expresses card-first.

## Reasoning
Reuse of the NfcReader/WifiService/ApiClient/PresenceCore/Feedback
seams keeps the refactor a re-composition, not a rewrite; the station
transaction reuses the backend contract B2B-Core already pins.

## Consequences
- `esp32cam` = station. Reader DevKit (`esp32dev`) untouched.
- Camera-only serial flows (ENTER/`a`/`e`/`c`, visualizer) preserved.
- GPIO4 = RST and buzzer = -1 are now load-bearing invariants, pinned
  by `test_station_config.cpp` static_asserts.

## Status
ACTIVE
