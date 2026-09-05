# Presence Platform — Reader Firmware (ESP32)

> Also available in: [Español](README.es.md)
> Companion repos: [B2B-Core](https://github.com/Nexusdeveloper902/B2B-Core) (backend — read-only reference except the TASK-010 pairing endpoint)

ESP32 firmware for the physical NFC card readers of the Presence Platform,
built as a **PlatformIO project** for the **Arduino framework**. Each
reader makes an authenticated HTTP call to the B2B-Core backend — the same
call a human would previously have sent from Postman. The backend needs
zero changes for real hardware (by design); the one exception, the card
pairing endpoint, was built in B2B-Core as `TASK-010` specifically to
unblock this firmware.

## What it does

Two operating modes. The device boots in **Operation**; the operator
toggles to Pairing and back at any time by typing the **mode password**
(stored in the gitignored `secrets.h`) + Enter in the Serial Monitor:

| Mode | Behavior |
|---|---|
| **Operation** (boot default) | Tap a paired card → `POST /api/v1/events/tap` → presence event + feedback |
| **Pairing** | Tap a fresh card → `POST /api/v1/admin/cards/pair` → card linked to the armed student — full operator guide: [docs/PAIRING.md](docs/PAIRING.md) |

- **Mode switching**: serial-console password (`MODE_PASSWORD` in secrets.h) toggles OPERATION <-> PAIRING at runtime — 2 slow EVENT-LED blinks confirm the switch; 3 wrong passwords lock the console for 10 s (configurable). Typed characters echo as `*`.
- **The device teaches its own flow** (TASK-004): every mode switch prints a bilingual hint of what to do next, and 401/409/422/404 responses print remediation lines — e.g. the 401 points at the reader-key provisioning steps in [docs/PAIRING.md](docs/PAIRING.md).

- **NFC**: RC522 over SPI — the default build since TASK-002 (`pio run -t upload` flashes the real reader). A serial mock reader remains available as an opt-in dev env (`-e esp32dev-mock`).
- **Identity**: static Bearer `READER_API_KEY` — the key IS the reader identity.
- **Feedback**: continuous mode LED + event LED patterns + optional buzzer; every outcome (success / 404 / 409 / 422 / 401 / network failure) has a distinct pattern and a bilingual serial log.
- **Resilience**: debounced reads, non-blocking `millis()` loop, bounded Wi-Fi connect with background reconnect, bounded HTTP timeouts — the device never hangs or crashes on failure.

## Requirements

- [PlatformIO](https://platformio.org/) (`pip install platformio`)
- An ESP32 dev board (generic `esp32dev` assumed) + RC522 module + LEDs (see [docs/HARDWARE_SETUP.md](docs/HARDWARE_SETUP.md)); a computer with the Serial Monitor for mode switching
- A running [B2B-Core](https://github.com/Nexusdeveloper902/B2B-Core) backend (`./run setup && ./run serve`) and the reader `api_key` its seeder prints

## Quick start

```bash
# 1. secrets (never committed)
cp include/secrets.h.example include/secrets.h
$EDITOR include/secrets.h          # WIFI_SSID, WIFI_PASSWORD, API_BASE_URL, READER_API_KEY, MODE_PASSWORD

# 2. compile (RC522 real reader — the DEFAULT env since TASK-002)
#    wiring: SCK=18 MISO=19 MOSI=23 SDA/SS=5 RST=27 (docs/HARDWARE_SETUP.md)
pio run

# 3. host-side unit tests (no hardware needed)
pio test -e native

# 4. flash + monitor  (plain `pio run -t upload` flashes the real reader)
pio run -e esp32dev -t upload && pio device monitor
```

On a healthy boot the monitor prints
`[NFC] RC522 detected — firmware version 0x92 / detectado` — the positive
confirmation the reader radio is alive. If the RC522 is missing/miswired,
the firmware logs the probed version + expected pins and **keeps retrying
init every 5 s** (fix the wiring with the device running — no reboot).

### No reader attached? Development build (opt-in)

```bash
pio run -e esp32dev-mock -t upload && pio device monitor
```

With the **mock build** flashed (even to a bare board), type a card UID +
Enter in the Serial Monitor to simulate a tap — the full pipeline runs.

### Backend integration E2E (no hardware needed)

```bash
# against a local B2B-Core checkout (uses throwaway DB + real HTTP)
B2B_CORE=../B2B-Core B2B_PHP=php ./scripts/e2e_backend.sh
```

The harness sends the firmware's **byte-identical payloads** (built by the
firmware's own `PayloadBuilder`) to a real running backend, captures every
documented response case, and feeds the **real responses** through the
firmware's own `ResponseParser` — 8/8 verdicts (tap + pairing, incl. 404 /
409 / 422 / 401).

## Documentation

| Doc | Contents |
|---|---|
| [docs/HARDWARE_SETUP.md](docs/HARDWARE_SETUP.md) | Wiring, pins, libraries, flashing, LED pattern table |
| [docs/PAIRING.md](docs/PAIRING.md) | Pairing mode end to end: arm-then-pair flow, reader-key provisioning, every outcome + fix, FAQ, security |
| [docs/API_INTEGRATION.md](docs/API_INTEGRATION.md) | The exact backend contract this firmware implements (both modes, all response cases) |
| [docs/MANUAL_VERIFICATION_CHECKLIST.md](docs/MANUAL_VERIFICATION_CHECKLIST.md) | Bench checklist for a human with the real board |
| [.agent/](.agent/PROJECT.md) | Agent protocol records (tasks, ADRs, runs, state) |

## Repository layout

```text
platformio.ini            build envs: esp32dev (default: real RC522) | esp32dev-mock | native
include/config.h          pins + timing (confirm against your wiring)
include/secrets.h.example credential template (secrets.h is gitignored)
src/main.cpp              thin composition root (wiring only)
lib/PresenceCore/         pure C++: payloads, parsers, modes, debounce, patterns, serial console
lib/NfcReader/            NfcReader interface + RC522 + serial mock
lib/Feedback/             FeedbackController interface + LED implementation
lib/ApiClient/            ApiClient interface + ESP32 HTTPClient transport
lib/WifiService/          bounded connect + non-blocking reconnect
test/                     host-side native unit tests (65)
tools/e2e/                E2E harness: firmware payloads + parser vs real backend
scripts/e2e_backend.sh    backend integration E2E (throwaway DB, real HTTP)
docs/                     bilingual real documentation (EN/ES)
```

## Development honesty

Most development happens without the board attached. What is verified
here: compilation for `esp32dev`, host unit tests of all
hardware-independent logic, and mock-reader behavior. What requires a
human at the bench: real RC522 reads, real Wi-Fi association, LED
behavior, serial-console typing — guided step by step by
[docs/MANUAL_VERIFICATION_CHECKLIST.md](docs/MANUAL_VERIFICATION_CHECKLIST.md).

## Security

Real Wi-Fi credentials, backend URLs and reader API keys are never
committed. `include/secrets.h` is gitignored; only the placeholder
template is tracked.
