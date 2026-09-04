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
| SCK       | **18** | hardware VSPI (fixed) |
| MOSI      | **23** | hardware VSPI (fixed) |
| MISO      | **19** | hardware VSPI (fixed) |
| RST       | **27** | `PIN_RC522_RST` |
| 3.3V / VCC | 3V3  | — |
| GND       | GND   | — |

> ⚠ WIRING CONFIRMATION: GPIO 5 / 27 are the defaults for SS/RST. Some
> RC522 breakouts and some ESP32 boards use different conventions (e.g.
> SS=21, RST=22). If the reader fails to init (the serial log prints
> `[!] NFC reader init failed`), re-check these two pins first and adjust
> `include/config.h`.

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
| `esp32dev-mock` (default) | `MockSerialNfcReader` — type a UID + Enter in the Serial Monitor | development without the RC522 attached; still exercises Wi-Fi, HTTP, modes, feedback on a real board |
| `esp32dev` | `Rc522NfcReader` (RC522 over SPI) | the real reader |
| `native` | — | host-side unit tests (`pio test -e native`) |

## Flashing

```bash
cp include/secrets.h.example include/secrets.h   # then edit: Wi-Fi, backend URL, reader key
pio run -e esp32dev -t upload                     # real reader
pio run -e esp32dev-mock -t upload                # mock reader
pio device monitor                                # 115200 baud
```

The reader key comes from the B2B-Core seeder output (`./run setup` in
the backend repo prints every reader's `api_key`).

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
| `MODE_BUTTON_SETTLE_MS` | 50 | boot-time button debounce |

All loop timing is `millis()`-based and non-blocking; there is no `delay()`
in `loop()`.
