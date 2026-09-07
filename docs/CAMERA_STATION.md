# ESP32-CAM Station Firmware — TASK-008+ (EN)

The complete recycling station in ONE device: an AI-Thinker ESP32-CAM
(OV3660) with camera + RC522 aboard. RFID taps resolve identity through
the presence tap endpoint; when the backend answers next_step
`awaiting_classification`, the station captures and classifies inside
the same transaction (card-first). ENTER / shutter stay bottle-first.
Camera init, JPEG capture, serial commands and HTTP visualizer are
merged from the known-working reference implementation
(`ESP32-CAM-CV/firmware/esp32_cam`).

The standalone reader firmware (RC522, `esp32dev` env) is preserved for
the DevKit board — see `docs/HARDWARE_SETUP.md` for that device.

## Station wiring (authoritative — bench-verified 2026-09-07)

| RC522 pin | ESP32-CAM GPIO | note |
|---|---|---|
| SDA (SS)  | **13** | SPI select |
| SCK       | **14** | SPI clock (SD CLK pin, SD unused) |
| MOSI      | **15** | SPI MOSI (SD CMD pin, SD unused) |
| MISO      | **2**  | SPI MISO (SD DATA0 pin, SD unused) |
| RST       | **4**  | RC522 reset (flash LED shares it — activity blinks are normal) |
| 3.3V / VCC | 3V3  | never 5 V |
| GND       | GND   | common ground |

Reserved / forbidden: **GPIO16/17 = PSRAM** (never RC522 RST);
**GPIO12 = shutter to GND only** (MTDI strapping — never 3V3);
**buzzer absent** (`PIN_CAM_BUZZER -1`: GPIO4 is RST, an idle-LOW
buzzer would hold the RC522 in reset); **SD card intentionally unused**
(never init `SD_MMC`/`SD` — the slot's pins are the SPI bus now).
Single status LED: red on **GPIO33** (active-LOW): heartbeat =
operation idle, double-blip = pairing, triple-blip = degraded
(camera/NFC/net down — station stays alive and retries), rapid =
connecting; solid 1.5 s = success event.

## What this firmware does

| Input | Action |
|---|---|
| RFID card tap | Presence tap → on `awaiting_classification` + event id: auto-capture + `POST /api/v1/recycling/classify` (card-first, one transaction) |
| `ENTER` (empty line) | Capture a high-res JPEG → `POST /api/v1/recycling/capture` (bottle-first: the backend holds the image `awaiting_card`; **no classifier call until a card resolves it** — the spec §4 cost gate) |
| `a <credential_uid>` | `POST /api/v1/recycling/captures/<last>/associate` — resolve the last capture with that card (event + classification + points, B2B-Core TASK-025) |
| `e <event_id>` | Arm card-first mode: the NEXT `ENTER` captures and `POST /api/v1/recycling/classify` with that event_id |
| `c` | Local capture only (no upload — dev/visualizer use) |

ENTER never multi-fires on key-repeat or buffered input (line discipline
+ 2 s cooldown, host-tested in `test/test_capture_trigger.cpp`). The
ENTER key is the *temporary* physical trigger — the future IR sensor
replaces it at the `CaptureTrigger` seam only (spec §37).

**Shutter button:** a momentary button between **GPIO12 and GND** (no
resistor — internal pull-up) fires the exact same flow as ENTER
(debounced, one photo per push, holding never refires). GPIO12 was
chosen because the camera bus, the flash LED, and your RC522
(13/14/15/2/4) are all elsewhere — see `config.h`. Never tie
GPIO12 HIGH (boot strapping).

**Buzzer:** absent on this bench (`PIN_CAM_BUZZER -1`). GPIO4 is RC522 RST (verified 2026-09-07), and a buzzer idling LOW would hold the active-LOW RC522 reset forever — so no buzzer until it moves off GPIO4. The flash LED shares GPIO4 and fires with RC522 RST activity — normal.

## Provisioning

1. `cp include/secrets.camera.h.example include/secrets.camera.h` and fill in (own file, separate from the reader's `secrets.h` — different backend identity):
   - `WIFI_SSID` / `WIFI_PASSWORD` — the school network.
   - `API_BASE_URL` — the B2B-Core host, e.g. `http://192.168.1.50:8000`
     (start it with `./run serve --host` so it is reachable from Wi-Fi).
   - `READER_API_KEY` — a **recycling** reader key as printed by
     B2B-Core's `./run setup`. The camera station IS a reader identity
     to the backend. With the demo seed there is exactly one recycling
     reader (`Demo Reader — Recycling`); sharing its key between the
     RC522 station and the camera station is fine (events and captures
     then belong to the same station row). For production, provision a
     dedicated `readers` row for the camera station.
2. Build, flash, monitor:

   ```
   pio run -e esp32cam -t upload
   pio device monitor -e esp32cam
   ```

   `monitor_dtr=0` / `monitor_rts=0` are already set in `platformio.ini`
   — **keep them**: the AI-Thinker auto-download circuit holds the chip
   in a bad state when the serial port is opened otherwise.

## Bench flow against a running B2B-Core (the exact script)

Prerequisites: B2B-Core seeded and serving (`./run setup && ./run serve`),
the camera station on Wi-Fi, its serial monitor open.

1. **Bottle-first (spec §3 Case B).** Place the bottle in view, press
   **ENTER** in the serial monitor. Expected serial output (abridged):

   ```
   HIGH-RES CAPTURE / CAPTURA ALTA RESOLUCIÓN
   Captured: 1024x768 / Capturado: 1024x768
   [EN] Bottle-first capture: uploading image (no card yet)...
   [EN] capture: HTTP 200
   {"status":"ok","capture_id":12,"state":"awaiting_card","expires_in":300,"next_step":"present_card"}
   [EN] Backend capture id 12 — now tap a card ('a <credential_uid>').
   ```

   The student taps their card; the operator types
   `a A1B2C3D4E5F6` (the `credential_uid` the seeder printed — or read
   from the RC522 station's own serial log). Expected:

   ```
   [EN] associate: HTTP 200
   {"status":"ok","capture_id":12,"capture_state":"accepted",...,"material_class":"plastic","points_awarded":10,"new_balance":10}
   ```

   The dashboard's leaderboard and the student's own desk update live
   (recycling WS frames, B2B-Core TASK-025 item 6).

2. **Card-first (spec §3 Case A).** Tap a card on the RC522 station and
   note the `event_id` in its serial log, then on the CAMERA monitor
   type `e 88`, place the bottle, press **ENTER**. Expected: the
   classify upload runs (`classify: HTTP 200`, `material_class`,
   `points_awarded`).

3. **Failure states.** A camera/Wi-Fi/backend failure prints
   `NETWORK ERROR (transport)` or the HTTP status + backend JSON body
   — never a silent skip. A capture with no card for 300 s simply
   expires server-side (no award, no leak — pinned by B2B-Core's
   CaptureFlowTest).

## Visualizer

Browse to the IP printed at boot: live view (`/stream-frame`), the last
high-res capture (`/capture.jpg`, `/capture-status`) — the reference's
operator eyes, preserved.

## Verification status (honesty section)

- Host-verified (this repository): 92/92 native tests (incl. station pin map + station feedback), all four build
  environments green (`esp32dev`, `esp32dev-mock`, `esp32cam`, fresh
  checkout without the secrets files compiles via the `__has_include` guard).
- Bench-verified by the owner: the flow above, against real hardware —
  per the repo's hardware-honesty convention
  (`docs/MANUAL_VERIFICATION_CHECKLIST.md`).
- The backend endpoints themselves are pinned by B2B-Core's own test
  suite (CaptureFlowTest + e2e Fase H) — see that repo's
  `docs/API.md` for the exact contracts.
