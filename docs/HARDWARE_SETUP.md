# Hardware Setup — Presence Platform Reader (ESP32 + RC522)

> También disponible en: [Español](HARDWARE_SETUP.es.md)

This document describes the physical wiring, the build environments, and
the flashing procedure for the reader firmware. **Confirm every pin
against your actual wiring before relying on it** — the defaults assume a
generic ESP32 DevKit (`esp32dev`) and can be changed in
`include/config.h`.

## Bill of materials

| Part | Notes |
|---|---|
| ESP32 DevKit (esp32dev-compatible) | any board exposing VSPI + free GPIOs |
| RC522 NFC module | 13.56 MHz, SPI interface, 3.3 V |
| LED 1 — "MODE" | indicates the current operating mode, continuously |
| LED 2 — "EVENT" | plays the tap/pair result patterns |
| (optional) passive buzzer | success chirp only |
| Momentary button | to GND — hold while powering on to enter PAIRING mode |
| 2 × resistor (220–470 Ω) | LED series resistors |
| Breadboard + wires | |

## Wiring table

⚠ **Read this before wiring:** the RC522 is a 3.3 V device — power it from
the ESP32's 3V3 pin, never 5 V/VIN.

### RC522 (SPI — uses the ESP32 VSPI bus)

| RC522 pin | ESP32 GPIO | config.h constant |
|---|---|---|
| SDA (SS)  | **5**  | `PIN_RC522_SS` |
| SCK       | **18** | `PIN_RC522_SCK` (VSPI clock) |
| MOSI      | **23** | `PIN_RC522_MOSI` (VSPI data out) |
| MISO      | **19** | `PIN_RC522_MISO` (VSPI data in) |
| RST       | **27** | `PIN_RC522_RST` |
| 3.3V / VCC | 3V3  | — |
| GND       | GND   | — |

> ⚠ WIRING CONFIRMATION: all five RC522 signal pins are configurable in
> `include/config.h` (defaults above). Some RC522 breakouts and some ESP32
> boards use different conventions (e.g. SS=21, RST=22). If the reader is
> not detected, the serial log prints the probed pins + expected ones in
> the `[NFC] RC522 NOT responding (...)` line — re-check the wiring
> against that line and adjust `config.h`.
>
> ✅ On a healthy boot the log prints `[NFC] RC522 detected — firmware
> version 0x92 / detectado` (0x91 = v1.0, 0x92 = v2.0, 0x90/0x88 = some
> clones). That line is your positive confirmation the radio is alive.

### Mode-select button

| Connection | config.h constant | Meaning |
|---|---|---|
| GPIO **32** ↔ button ↔ GND | `PIN_MODE_SELECT` | `INPUT_PULLUP` enabled |

Logic level at boot (after 50 ms settle / double-read debounce):

| Pin level at boot | Selected mode |
|---|---|
| HIGH (button not pressed) | **OPERATION MODE** (normal taps) |
| LOW (button held during power-on/RESET) | **PAIRING MODE** (pair a new card) |

The mode is fixed for the session; to change it, hold the button and press
EN/RESET. The MODE LED always shows which mode is active (see pattern
table below), so you never have to guess.

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
| Unexpected server response | EVENT | long solid 2 s |

The serial monitor prints a bilingual line for every event (see the
`[NFC]` / `[OK]` / `[404]` / `[409]` / `[422]` / `[401]` / `[NET]` /
`[ERR]` prefixes), so patterns and logs confirm each other.

## Build environments

| Environment | Reader | Use |
|---|---|---|
| `esp32dev` (**default**) | `Rc522NfcReader` (RC522 over SPI) | the real reader — `pio run` and `pio run -t upload` target it since TASK-002 |
| `esp32dev-mock` (opt-in) | `MockSerialNfcReader` — type a UID + Enter in the Serial Monitor | development without the RC522 attached; still exercises Wi-Fi, HTTP, modes, feedback on a real board |
| `native` | — | host-side unit tests (`pio test -e native`) |

## Flashing

```bash
cp include/secrets.h.example include/secrets.h   # then edit: Wi-Fi, backend URL, reader key
pio run -e esp32dev -t upload                     # real reader (default env since TASK-002)
#   equivalent to: pio run -t upload
pio run -e esp32dev-mock -t upload                # mock reader (opt-in)

pio device monitor                                # 115200 baud
```

The reader key comes from the B2B-Core seeder output (`./run setup` in
the backend repo prints every reader's `api_key`).

## Reader self-recovery (TASK-002)

The RC522 driver tracks its own health: if init fails at boot (wiring,
power) or the reader stops answering at runtime (glitch, ESD,
brown-out), the firmware retries `PCD_Init` every 5 s
(`RC522_REINIT_INTERVAL_MS`) — non-blocking, no reboot needed. Fix the
wiring while the device runs and the `[NFC] RC522 detected ...` line
appears at the next retry. Boot lines of the real-reader build:

```text
Reader impl / Implementacion: RC522 (SPI)
[NFC] RC522 detected — firmware version 0x92 / detectado
---- present a card to the reader / presenta una tarjeta al lector ----
```

The mock build instead prints `---- type a UID + Enter ... ----`.

## Libraries (managed by PlatformIO, `platformio.ini`)

| Library | Version | Purpose |
|---|---|---|
| `bblanchon/ArduinoJson` | ^7.0.0 | request/response JSON (host-testable too) |
| `miguelbalboa/rfid` | ^1.6.4 | MFRC522/RC522 driver (env `esp32dev` only) |

## Timing constants (config.h)

| Constant | Default | Meaning |
|---|---|---|
| `WIFI_CONNECT_TIMEOUT_MS` | 15000 | bounded initial Wi-Fi association |
| `WIFI_RECONNECT_INTERVAL_MS` | 10000 | background retry cadence after a drop |
| `HTTP_TIMEOUT_MS` | 10000 | per-request HTTP timeout |
| `CARD_COOLDOWN_MS` | 2000 | same-UID re-read debounce window |
| `RC522_REINIT_INTERVAL_MS` | 5000 | RC522 init retry cadence while the reader is not answering (self-recovery) |
| `MODE_BUTTON_SETTLE_MS` | 50 | boot-time button debounce |

All loop timing is `millis()`-based and non-blocking; there is no `delay()`
in `loop()`.
