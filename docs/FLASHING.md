# Flashing — `scripts/flash.sh`

> También disponible en: [Español](FLASHING.es.md)

`scripts/flash.sh` is the per-board flashing wrapper: one command to build
and upload the right firmware to the right board, optionally followed by
the serial monitor. It wraps

```bash
pio run -e <env> -t upload [--upload-port <port>] [extra pio args...]
```

## Why it exists

ESP32 DevKits (the RC522 reader) and ESP32-CAM boards (the camera
station) expose **identical USB-UART bridges**, so the board type CANNOT
be auto-detected from the serial port. The flag you pass IS the selector.
A bare `pio run -t upload` builds the PlatformIO `default_envs`
(`esp32cam`) — flashing an `esp32dev` image onto a CAM board drives
camera-bus pins as SPI, the exact wrong-target failure these flags
prevent (see ADR-010).

## Board ↔ env mapping

| Short flag | `--board` / `--env` / `-e` value | Env | Target |
|---|---|---|---|
| *(no flag)* | `esp32cam` / `cam` | `esp32cam` | **ESP32-CAM recycling station** — the default, matching `default_envs` in `platformio.ini` (station-first since ADR-010) |
| `--esp32` / `--reader` | `esp32` / `reader` / `esp32dev` | `esp32dev` | ESP32 DevKit RC522 reader |
| `--cam-reader` | `cam-reader` / `esp32cam-reader` | `esp32cam-reader` | ESP32-CAM board running **only the reader** — RC522 on the station's pins, onboard LED, no camera code ([HARDWARE_SETUP.md](HARDWARE_SETUP.md#reader-on-an-esp32-cam-board-esp32cam-reader)) |
| `--mock` | `mock` / `esp32dev-mock` | `esp32dev-mock` | Mock reader — serial-driven virtual taps, development without the RC522 attached |

> Since ADR-010 the default is the **camera station**, not the reader.
> Reader DevKit operators must pass `--esp32` explicitly.

## Usage

```bash
./scripts/flash.sh                      # camera station (esp32cam — default)
./scripts/flash.sh --esp32              # reader DevKit (esp32dev)
./scripts/flash.sh --reader             # same as --esp32
./scripts/flash.sh --cam-reader -m      # reader on an ESP32-CAM board (esp32cam-reader), then monitor
./scripts/flash.sh --mock               # mock reader (esp32dev-mock)
./scripts/flash.sh --board esp32cam     # long form (also: --env / -e)
./scripts/flash.sh --esp32 --port /dev/ttyUSB1
./scripts/flash.sh --esp32 --monitor    # flash, then open the serial monitor
./scripts/flash.sh --cam -m             # flash the station, then monitor
./scripts/flash.sh --cam -- --upload-port COM3   # anything after `--` is forwarded to pio
```

## Options

| Option | Effect |
|---|---|
| `--esp32` / `--reader` | target `esp32dev` (real RC522 reader) |
| `--esp32cam` / `--cam` | target `esp32cam` (camera station — also the no-flag default) |
| `--cam-reader` | target `esp32cam-reader` (reader image on an ESP32-CAM board, no camera) |
| `--mock` | target `esp32dev-mock` (mock reader) |
| `--board <name>` / `--env <name>` / `-e <name>` | long form of the above: accepts `esp32` \| `reader` \| `esp32dev`, `esp32cam` \| `cam`, `cam-reader` \| `esp32cam-reader`, `mock` \| `esp32dev-mock` |
| `--port <port>` | forward `--upload-port <port>` to `pio run` (e.g. `/dev/ttyUSB1`, `COM3`) |
| `-m` / `--monitor` | after a successful flash, `exec pio device monitor -e <env>` (115200 baud) |
| `--` | everything after is appended to the `pio run` command unchanged |
| `-h` / `--help` | print the in-script usage block |
| env `DRY_RUN` | print the `pio` command without running it — `DRY_RUN=1 ./scripts/flash.sh --esp32` (no hardware needed) |

Unknown flags/boards exit with code 2 and a hint listing the valid names.

## Monitor notes

- The monitor runs **with the target env's config**: for the camera
  station (and `esp32cam-reader`) that keeps `monitor_dtr=0` / `monitor_rts=0`, which the
  AI-Thinker auto-download circuit requires — never open the CAM port
  with a plain env-less `pio device monitor`.
- Baud is 115200 (`monitor_speed` in `platformio.ini`).
- Once in the monitor, interact as documented per board: type the mode
  password (reader/station) or a card UID + Enter (mock).

## Before you flash

- Secrets must be provisioned for the board you target: the DevKit reader
  uses `include/secrets.h` (from `secrets.h.example`); the CAM reader uses
  `include/secrets.cam_reader.h` (from `secrets.cam_reader.h.example`,
  different `READER_API_KEY`); the camera station uses
  `include/secrets.camera.h` (from `secrets.camera.h.example`). See `docs/HARDWARE_SETUP.md` /
  `docs/CAMERA_STATION.md`.
- `pio` (PlatformIO) must be on `PATH` — otherwise the script prints
  `platformio not found (pip install platformio)` and exits 1.
- No board selected twice: pick one short flag or one `--board` value.

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Script flashes the wrong image | The no-flag default is the **camera station** — pass `--esp32` for a reader DevKit, `--mock` for a virtual-tap build |
| `unknown board: ...` | Board name typo — allowed: `esp32` \| `esp32cam` \| `mock` (and aliases above) |
| CAM won't flash / stays in a bad state after the monitor | Ensure the env is `esp32cam` (`--cam`) so `monitor_dtr`/`monitor_rts` stay 0 |
| `platformio not found` | `pip install platformio` |

For wiring, pin maps, secrets, and the serial mode console, see
[docs/HARDWARE_SETUP.md](HARDWARE_SETUP.md) (reader) and
[docs/CAMERA_STATION.md](CAMERA_STATION.md) (station).
