# Hardware Setup — Presence Platform Reader (ESP32 + RC522)

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

### RC522 (SPI — uses the ESP32 VSPI bus)

| RC522 pin | ESP32 GPIO | config.h constant |
|---|---|---|
| SDA (SS)  | **13** | `PIN_RC522_SS` |
| SCK       | **14** | `PIN_RC522_SCK` (VSPI clock) |
| MOSI      | **15** | `PIN_RC522_MOSI` (VSPI data out) |
| MISO      | **2**  | `PIN_RC522_MISO` (VSPI data in) |
| RST       | **4**  | `PIN_RC522_RST` (bench-verified 2026-09-07: DIAG-CAM VersionReg 0x92 stable + raw agree; GPIO16 is PSRAM CS on ESP32-CAM — never RST) |
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

## Build environments

| Environment | Reader | Use |
|---|---|---|
| `esp32dev` (**default**) | `Rc522NfcReader` (RC522 over SPI) | the real reader — `pio run` and `pio run -t upload` target it since TASK-002 |
| `esp32dev-mock` (opt-in) | `MockSerialNfcReader` — type a UID + Enter in the Serial Monitor | development without the RC522 attached; still exercises Wi-Fi, HTTP, modes, feedback on a real board |
| `native` | — | host-side unit tests (`pio test -e native`) |

## Flashing

```bash
cp include/secrets.h.example include/secrets.h   # then edit: Wi-Fi, backend URL, reader key, MODE_PASSWORD
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
| `MODE_CONSOLE_MAX_WRONG_ATTEMPTS` | 3 | wrong mode passwords before the console locks |
| `MODE_CONSOLE_LOCKOUT_MS` | 10000 | mode-console lock duration |
| `SERIAL_LINE_MAX_LENGTH` | 64 | serial input line cap (longer lines discarded) |

All loop timing is `millis()`-based and non-blocking; there is no `delay()`
in `loop()`.
