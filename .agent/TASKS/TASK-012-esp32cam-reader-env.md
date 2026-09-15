# TASK-012 — `esp32cam-reader`: reader-only image for an ESP32-CAM board

## Date opened
2026-09-15

## Origin
Owner: "another profile that uses the same rc522 pins as the esp32 cam;
the physical thing is an esp32cam but it behaves only as an esp32, no
camera logic, the camera won't even be connected."

## Delivered
- `[env:esp32cam-reader]` in platformio.ini (ADR-014).
- `include/config/esp32cam_board.h` (shared RC522 + LED pins, extracted
  from `esp32cam.h`), `include/config/esp32cam_reader.h`, dispatcher case
  in `include/config.h`.
- `src/main.cpp`: `StationLed` on GPIO33 under `READER_ON_CAM_BOARD`,
  banner board line, wiring hint.
- `scripts/flash.sh --cam-reader`; HARDWARE_SETUP + FLASHING (EN + ES).
- `test/test_cam_reader_config.cpp` (compile-time pin map guard).

## Verification
- `pio test -e native` 112/112.
- `pio run -e esp32cam-reader` SUCCESS; `nm` shows 0 `esp_camera` /
  `Station::` symbols. `esp32cam`, `esp32dev`, `esp32dev-mock` SUCCESS.
- `DRY_RUN=1 scripts/flash.sh --cam-reader` → `pio run -e esp32cam-reader -t upload`.
- Pending: bench flash on a CAM board with the RC522 harness.
