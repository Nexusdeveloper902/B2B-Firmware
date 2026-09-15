# Hardware Setup — Pulse Reader (ESP32 + RC522)

> También disponible en: [Español](HARDWARE_SETUP.es.md)

This document describes the physical wiring, the build environments, the
flashing procedure, and the serial mode console for the reader firmware.
**Confirm every pin against your actual wiring before relying on it** —
the defaults assume a generic ESP32 DevKit (`esp32dev`) and can be
changed in `include/config.h`.

## Bill of materials

| Part | Notes |
|---|---|
| ESP32 DevKit (esp32dev-compatible) | any board exposing VSPI + free GPIOs |
| RC522 NFC module | 13.56 MHz, SPI interface, 3.3 V |
| LED 1 — "MODE" | indicates the current operating mode, continuously |
| LED 2 — "EVENT" | plays the tap/pair result patterns |
| (optional) passive buzzer | success chirp only |
| 2 × resistor (220–470 Ω) | LED series resistors |
| Breadboard + wires | |

> The mode-select BUTTON is no longer part of the BOM: since TASK-003
> mode switching happens over the Serial Monitor (password), no physical
> button is wired. GPIO 32 is free for future use. / El BOTÓN de modo ya
> no es parte de la lista: desde TASK-003 el cambio de modo es por el
> Monitor Serial (contraseña); el GPIO 32 queda libre.

## Wiring table

⚠ **Read this before wiring:** the RC522 is a 3.3 V device — power it from
the ESP32's 3V3 pin, never 5 V/VIN.

### RC522 (SPI — reader board, env `esp32dev`)

| RC522 pin | ESP32 GPIO | config constant (`include/config/esp32dev.h`) |
|---|---|---|
| SDA (SS)  | **5**  | `PIN_RC522_SS` |
| SCK       | **18** | `PIN_RC522_SCK` (VSPI clock) |
| MOSI      | **23** | `PIN_RC522_MOSI` (VSPI data out) |
| MISO      | **19** | `PIN_RC522_MISO` (VSPI data in) |
| RST       | **27** | `PIN_RC522_RST` (bench-verified 2026-09-07) |
| 3.3V / VCC | 3V3  | — |
| GND       | GND   | — |

> ⚠ NOT the camera-station map: the ESP32-CAM station (default env
> `esp32cam`) wires the RC522 differently (SS 13 / SCK 14 / MOSI 15 /
> MISO 2 / RST strapped 3V3 — GPIO4 there is the flash LED, GPIO16 is
> PSRAM CS) because those are the only free pins on that board. The
> station's authoritative table lives in `docs/CAMERA_STATION.md`.
>
> ⚠ WIRING CONFIRMATION: all five RC522 signal pins are configurable in
> `include/config/esp32dev.h` (defaults above; timing knobs live in
> `include/config/common.h`). Some RC522 breakouts and some ESP32 boards
> use different conventions (e.g. SS=21, RST=22). If the reader is not
> detected, the serial log prints the probed pins + expected ones in the
> `[NFC] RC522 NOT responding (...)` line — re-check the wiring against
> that line and adjust the config header.
>
> ✅ On a healthy boot the log prints `[NFC] RC522 detected — firmware
> version 0x92 / detectado` (0x91 = v1.0, 0x92 = v2.0, 0x90/0x88 = some
> clones). That line is your positive confirmation the radio is alive.

### Mode switching — Serial Monitor password (TASK-003)

No wiring required. The device boots in OPERATION MODE; type the mode
password + Enter in the Serial Monitor (115200 baud) to toggle
OPERATION <-> PAIRING at any time. The password value lives in the
gitignored `include/secrets.h` (`MODE_PASSWORD` — see
`secrets.h.example`); the knobs live in `config.h`:

| Constant | Default | Meaning |
|---|---|---|
| `MODE_CONSOLE_MAX_WRONG_ATTEMPTS` | 3 | wrong passwords before the console locks |
| `MODE_CONSOLE_LOCKOUT_MS` | 10000 | how long the lock lasts |
| `SERIAL_LINE_MAX_LENGTH` | 64 | input line cap (longer lines are discarded) |

Console behavior:

- Typed characters echo as `*` (masked — the password never appears on
  screen), and the log never prints the expected password.
- Correct password → `[MODE] switched to / cambiado a: ...` + 2 slow
  EVENT-LED blinks + the MODE LED immediately shows the new idle pattern.
- Wrong password → `[MODE] wrong password / clave incorrecta — N
  attempt(s) left` + 2 very fast EVENT-LED blinks. After 3 wrongs the
  console locks for 10 s (`[MODE] input locked ...`), even for the
  correct password, then resets.
- Real-reader build: every non-empty line is a password attempt.
- Mock build: a line that is not the password is a virtual card tap
  (UID), so the lockout never triggers on normal UID typing.
- Security honesty: the Serial Monitor is a physical-access (USB)
  channel — the password is an operator gate, not cryptography.
  Pairing still requires an admin-armed backend session (45 s window)
  plus the reader Bearer key.

### Feedback outputs

| Part | ESP32 GPIO | config.h constant |
|---|---|---|
| MODE LED (anode) → resistor → GPIO **25** | 25 | `PIN_LED_MODE` |
| EVENT LED (anode) → resistor → GPIO **26** | 26 | `PIN_LED_EVENT` |
| Buzzer (+) → GPIO **33** (optional; set `PIN_BUZZER` to `-1` if absent) | 33 | `PIN_BUZZER` |

LED cathodes → GND.

## LED pattern reference

The MODE LED blinks CONTINUOUSLY to show the state; the EVENT LED plays a
one-shot pattern whenever a card is processed (then stays off).

| Meaning | LED | Pattern |
|---|---|---|
| Boot / Wi-Fi connecting | MODE | rapid blink (100 ms on / 100 ms off) |
| Idle — OPERATION mode | MODE | 1 short blip every 2 s |
| Idle — PAIRING mode | MODE | 2 short blips every 2 s |
| Tap success (event logged) | EVENT | solid 1.5 s |
| Pair success (card linked) | EVENT | solid 1.5 s (+ buzzer chirp if present) |
| Card not recognized (404) | EVENT | 2 blinks (200 ms on/off) |
| No pairing session active (409) | EVENT | 3 blinks |
| Card already paired (422) | EVENT | 4 blinks |
| Network failure | EVENT | 5 fast blinks (120 ms) |
| Auth failure (401, bad reader key) | EVENT | 6 fast blinks (120 ms) |
| Mode switched (correct password) | EVENT | 2 slow blinks (500 ms on / 250 ms off) |
| Wrong mode password | EVENT | 2 very fast blinks (80 ms) |
| Unexpected server response | EVENT | long solid 2 s |

The serial monitor prints a bilingual line for every event (see the
`[NFC]` / `[OK]` / `[404]` / `[409]` / `[422]` / `[401]` / `[NET]` /
`[MODE]` / `[ERR]` prefixes), so patterns and logs confirm each other.

## Station LED diagnostics — "an LED is unexpectedly ON" (TASK-010 follow-up)

The ESP32-CAM station has TWO LEDs and exactly three ways one can light
"by itself". The station's red LED is the MODE+EVENT LED in one (the CAM
board has no second free LED):

| Symptom | Root cause | Fix |
|---|---|---|
| **White flash LED solid ON** | RC522 RST still wired to GPIO4 (pre-2026-09-07 wiring), or the board runs a pre-fix build: the MFRC522 library holds RST HIGH after `PCD_Init` and re-pulses it every 5 s | Move the RST wire to **3V3** (firmware drives nothing — `PIN_RC522_RST -1`) and **reflash** with the current `esp32cam` env. A compile-time assert now forbids GPIO4 anywhere in the driven pin map |
| **Red LED SOLID ON** (no pattern) | Either the **reader** (`esp32dev`) image was flashed on the CAM board — that image idles GPIO33 LOW for an (absent) buzzer, and the red LED is active-LOW — or the board is an **inverted-polarity clone** | Reflash the **`esp32cam`** env (the default since ADR-010); if it is still solid on a genuine station build, set `PIN_STATION_LED_ACTIVE_LOW 0` in `include/config/esp32cam.h` and rebuild |
| **Red LED short blips** (1×/2×/3× per 2 s) | NOT a fault — that is the heartbeat grammar: 1 blip = OPERATION, 2 = PAIRING, 3 = degraded (camera down) | Read it as the station talking to you |

Polarity is now a **config value** (`PIN_STATION_LED_ACTIVE_LOW`,
default `1` = active-LOW, the genuine AI-Thinker wiring) instead of a
buried assumption — one define flips an inverted-polarity clone board.
`test/test_station_config.cpp` pins the pin number, the polarity define's
range, the no-collision invariants (LED vs RC522 bus vs shutter) and the
"GPIO4 is never a driven pin" rule; `station_idle_patterns_are_mostly_off`
additionally guarantees every idle pattern stays dark most of the cycle,
so "mostly on" can never ship silently.

## Build environments

> Since ADR-010 the repo's DEFAULT build env is the **camera station**
> (`esp32cam`, station-first), so a bare `pio run -t upload` flashes the
> station image, not this reader. The reader env is opt-in: always pass
> the env explicitly (`pio run -e esp32dev`) or use the one-command
> wrapper `scripts/flash.sh --esp32` (full board map: [docs/FLASHING.md](FLASHING.md)).

| Environment | Reader | Use |
|---|---|---|
| `esp32dev` (opt-in) | `Rc522NfcReader` (RC522 over SPI) | the real reader — `pio run -e esp32dev` / `scripts/flash.sh --esp32` |
| `esp32cam-reader` (opt-in) | `Rc522NfcReader` (RC522 over SPI) on an **ESP32-CAM board, camera-less** | the same reader firmware on an AI-Thinker ESP32-CAM — `pio run -e esp32cam-reader` / `scripts/flash.sh --cam-reader`. See [Reader on an ESP32-CAM board](#reader-on-an-esp32-cam-board-esp32cam-reader) |
| `esp32dev-mock` (opt-in) | `MockSerialNfcReader` — type a UID + Enter in the Serial Monitor | development without the RC522 attached; still exercises Wi-Fi, HTTP, modes, feedback on a real board |
| `native` | — | host-side unit tests (`pio test -e native`) |

The station's `esp32cam` env (camera + RC522 over SPI) is documented in
[docs/CAMERA_STATION.md](CAMERA_STATION.md).

## Flashing

```bash
cp include/secrets.h.example include/secrets.h   # then edit: Wi-Fi, backend URL, reader key, MODE_PASSWORD

# The reader env is OPT-IN since ADR-010 — the default env is the camera
# station `esp32cam`, and a bare `pio run -t upload` would flash the
# station image instead of this reader.
pio run -e esp32dev -t upload                     # real reader
./scripts/flash.sh --esp32                        # same, via the flash wrapper
pio run -e esp32cam-reader -t upload              # reader on an ESP32-CAM board (no camera)
./scripts/flash.sh --cam-reader                   # same
pio run -e esp32dev-mock -t upload                # mock reader (opt-in)
./scripts/flash.sh --mock                         # same

pio device monitor -e esp32dev                    # 115200 baud
```

The reader key comes from the B2B-Core seeder output (`./run setup` in
the backend repo prints every reader's `api_key`).

## Reader on an ESP32-CAM board (`esp32cam-reader`)

An AI-Thinker ESP32-CAM can run **only** the reader: env `esp32cam-reader`
builds the same `src/main.cpp` as `esp32dev` (tap/pair pipeline, mode
console, HCE) for the CAM board, with **no camera code and no
`esp32-camera` library** — the camera module does not need to be
connected. It is not the station: no capture, no shutter, no visualizer.

| | `esp32cam-reader` |
|---|---|
| RC522 wiring | **identical to the station** — SDA/SS → GPIO13, SCK → GPIO14, MOSI → GPIO15, MISO → GPIO2, RST → **3V3** (not a GPIO), VCC → 3V3, GND → GND. Both envs read these from `include/config/esp32cam_board.h`, so a station's RC522 harness moves over unchanged. |
| Feedback | onboard red LED on **GPIO33** only (active-LOW; `PIN_STATION_LED_ACTIVE_LOW` flips inverted clones). Mode heartbeat and one-shot event patterns share it — an event preempts the heartbeat, which resumes afterwards. Same patterns as the [LED pattern reference](#led-pattern-reference). |
| Buzzer | none — the CAM board has no free header GPIO for it |
| Untouched pins | GPIO4 (flash LED), GPIO12 (MTDI strap), GPIO16 (PSRAM), GPIO1/3 (serial console) |
| Monitor | `pio device monitor -e esp32cam-reader` (keeps `monitor_dtr=0`/`monitor_rts=0`, required by the AI-Thinker auto-download circuit) |

The boot banner adds `Board: ESP32-CAM (reader only, no camera …)` so
the serial log shows which image is on the board. Do **not** flash
`esp32dev` onto a CAM board: that image puts the RC522 on GPIO18/19/23/5
(not broken out) and drives GPIO25/26 (camera bus) and GPIO33 as LEDs/buzzer.

## Reader self-recovery (TASK-002)

The RC522 driver tracks its own health: if init fails at boot (wiring,
power) or the reader stops answering at runtime (glitch, ESD,
brown-out), the firmware retries `PCD_Init` every 5 s
(`RC522_REINIT_INTERVAL_MS`) — non-blocking, no reboot needed. Fix the
wiring while the device runs and the `[NFC] RC522 detected ...` line
appears at the next retry. Boot lines of the real-reader build:

```text
Reader impl / Implementacion: RC522 (SPI)
Mode / Modo: OPERATION / OPERACION
[NFC] RC522 detected — firmware version 0x92 / detectado
---- type the MODE PASSWORD + Enter to switch modes / escribe la
     CLAVE DE MODO + Enter para cambiar de modo (secrets.h) ----
---- present a card to the reader / presenta una tarjeta al lector ----
```

The mock build instead prints `---- type a UID + Enter ... ----`.

## Libraries (managed by PlatformIO, `platformio.ini`)

| Library | Version | Purpose |
|---|---|---|
| `bblanchon/ArduinoJson` | ^7.4.3 | request/response JSON (host-testable too) |
| `miguelbalboa/MFRC522` | ^1.4.11 | MFRC522/RC522 driver (envs `esp32dev` + `esp32cam`) |

## Timing constants (config.h)

| Constant | Default | Meaning |
|---|---|---|
| `WIFI_CONNECT_TIMEOUT_MS` | 15000 | bounded initial Wi-Fi association |
| `WIFI_RECONNECT_INTERVAL_MS` | 10000 | background retry cadence after a drop |
| `HTTP_TIMEOUT_MS` | 10000 | per-request HTTP timeout |
| `CARD_COOLDOWN_MS` | 2000 | same-UID re-read debounce window |
| `RC522_REINIT_INTERVAL_MS` | 5000 | RC522 init retry cadence while the reader is not answering (self-recovery) |
| `MODE_CONSOLE_MAX_WRONG_ATTEMPTS` | 3 | wrong mode passwords before the console locks |
| `MODE_CONSOLE_LOCKOUT_MS` | 10000 | mode-console lock duration |
| `SERIAL_LINE_MAX_LENGTH` | 64 | serial input line cap (longer lines discarded) |

All loop timing is `millis()`-based and non-blocking; there is no `delay()`
in `loop()`.
