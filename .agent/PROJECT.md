# Project: Presence Platform — Reader Firmware

## What this is
ESP32 firmware for the physical NFC card readers in the Presence Platform.
This is the hardware side of the same system whose backend lives in the
separate `b2b-core` repository (GitHub: Nexusdeveloper902/B2B-Core). The
firmware's job is to make an authenticated HTTP call shaped exactly like
what a human would previously have sent from Postman — the backend was
deliberately designed so that wiring up real hardware requires no backend
changes, EXCEPT for the pairing endpoint, which did not exist and was added
as an explicitly authorized, narrowly-scoped exception (TASK-010 in
b2b-core, referenced from this repo's TASK-001).

Built as a PlatformIO project for an ESP32 (esp32dev board, Arduino
framework). Docs and serial output are bilingual English/Spanish, matching
the platform-wide convention (b2b-core ADR-008).

## NFC hardware
Confirmed: RC522 (SPI). No swappability hedge needed for the chip choice
itself, though the reader abstraction (Phase B) remains a clean interface
regardless. A serial mock reader exists for development without hardware.

## Two operating modes
1. PAIRING MODE — associates a newly scanned card's UID with a student
   record (calls the pairing endpoint built in b2b-core TASK-010). The
   full operator guide — arm-then-pair flow, reader-key provisioning,
   per-outcome troubleshooting — is docs/PAIRING.md + .es.md (TASK-004).
   Its Prerequisites also pin where the real seeded `credential_uid`s
   live (the cards table every `./run setup` prints, right above the
   readers table) and what 401 vs 404 means for the key check (a 404
   means the key was ACCEPTED; TASK-005). Since B2B-Core TASK-011 the guide's RECOMMENDED arming
   path is that repo's dashboard pairing desk ("Pair cards" page,
   session login, one click — TASK-006); curl + PAT stays the
   automation alternative.
2. OPERATION MODE — normal use: tap a paired card, log a presence event
   (calls POST /api/v1/events/tap).

Mode switching (TASK-003): the device boots in OPERATION and the operator
toggles OPERATION <-> PAIRING at any time by typing the mode password
(MODE_PASSWORD, secrets.h) in the Serial Monitor; the current mode shows
continuously on the mode LED. The boot-time mode button is gone (ADR-005,
superseding ADR-002).

## Device-to-backend auth (TASK-007 / ADR-007)
Every request carries `Authorization: Bearer <READER_API_KEY>` — the
value built by `Presence::bearerAuthorizationValue()` (PresenceCore,
pinned by test_auth.cpp) and sent via `addHeader` in EspApiClient.
`HTTPClient::setAuthorization(key)` is FORBIDDEN here: it prefixes the
default type "Basic" and the backend ignores that header — until
TASK-007 (2026-09-05) every real-hardware call answered 401 with a
perfectly valid key, invisible to curl-based verification. A 401 on
current firmware genuinely means the key has no readers row
(PAIRING.md §2).

## Explicit non-goals (for now)
- No camera / recycling-classification flow.
- No remote reconfiguration of a reader's event-type "mode" from firmware.
- No captive-portal Wi-Fi provisioning UI.
- No OTA updates.

## Relationship to other repos
b2b-core is read-only reference EXCEPT for the one authorized
pairing-endpoint exception (completed as b2b-core TASK-010, merged to its
main). The marketplace repository is out of scope entirely.

## Hardware honesty
Development happens without the physical board attached in most agent
runs. Compilation and host-testable logic are verified here;
hardware-in-the-loop behavior is verified by a human against a written
checklist (docs/MANUAL_VERIFICATION_CHECKLIST.md + .es.md).

## Toolchain
- PlatformIO (pip install platformio), espressif32 platform, Arduino
  framework, ArduinoJson + MFRC522 libraries.
- `pio run` compiles for esp32dev; `pio test -e native` runs host-side
  unit tests of the hardware-independent logic.
- Real credentials live only in the gitignored `include/secrets.h`
  (template: `include/secrets.h.example`).

## TASK-008 additions (2026-09-07, RUN-2026-09-07-firmware-010)

This repo now builds the recycling station's CAMERA half alongside the
reader: `[env:esp32cam]` (AI-Thinker ESP32-CAM, OV3660) — the verified
reference's camera init/capture/visualizer, plus the upload wiring to
B2B-Core TASK-025's endpoints:

- Serial: ENTER = capture + POST /recycling/capture (bottle-first);
  `a <uid>` = POST /recycling/captures/{id}/associate (resolution);
  `e <event_id>` = arm card-first classify; `c` = local capture.
- The trigger is the CaptureTrigger seam (spec §37): ENTER today, IR
  sensor tomorrow — swap at the interface only. No multi-fire (line
  discipline + 2 s cooldown, host-tested).
- Multipart bodies + Bearer value live in host-tested PresenceCore
  (CapturePayload, CaptureTrigger, buildAssociatePayload); associate
  rides EspApiClient (the TASK-007 Bearer lesson, one auth path).
- Reader envs build src/main.cpp only (src filters); reader behavior is
  byte-for-byte untouched. Bench script: docs/CAMERA_STATION.md (+.es).

## TASK-009 additions (2026-09-07, RUN-2026-09-07-firmware-011)

`esp32cam` is now the complete STATION in one device (ADR-009): a
`Station` lifecycle (`src/camera/station.h/.cpp`, thin main) owns
camera + RC522 + Wi-Fi + presence tap pipeline + capture trigger +
feedback + visualizer, with recoverable per-subsystem states (camera
re-init 30 s, NFC 5 s, Wi-Fi via WifiService — no FATAL halts). A tap
answered `awaiting_classification` + event id auto-captures and
classifies in the same transaction (existing `classifyWithEvent`
path); ENTER/shutter stay bottle-first; pairing, console password and
all prior flows preserved.

Hardware authority is `include/config/{common,esp32dev,esp32cam}.h`
with `config.h` dispatching on `-DCAMERA_STATION` (`config_camera.h`
deleted): RC522 13/14/15/2/4 (bench-verified), buzzer -1 (GPIO4 is
RST), shutter 12→GND, status LED 33 (active-LOW), PSRAM 16/17
reserved, no SD. Pinned by `test_station_config.cpp` (static_asserts;
native 96/96). Default build is the station (ADR-010, supersedes
ADR-004's default clause); DevKit via `-e esp32dev` / `flash.sh
--esp32`. Secrets stay split (`secrets.h` vs `secrets.camera.h`).

## TASK-010 delivery closed (2026-09-08, RUN-2026-09-08-firmware-012)

- The station LED "unexpectedly on" report decomposed into three root
  causes: (1) pre-e75e190 GPIO4-as-RST wiring (fixed in source since
  e75e190 — now FORBIDDEN by compile-time asserts), (2) wrong-target
  esp32dev image on the CAM board or inverted-polarity clone boards
  (now a one-line config: `PIN_STATION_LED_ACTIVE_LOW`, ADR-011), (3)
  the normal heartbeat grammar misread as a fault (now documented
  bilingually in HARDWARE_SETUP + CAMERA_STATION).
- Durable trap: pin-map constants buried in C++ constructors
  (station.cpp's hardcoded activeLow=true with a "confirm polarity"
  comment) are exactly where board-clone incompatibilities hide —
  board electrical constants belong in the board config header, and
  the pin map needs compile-time asserts (GPIO4-never-driven is the
  assert that would have caught the original flash-LED bug at build
  time).
- Verification: native 93/93 (+1 pattern-dominance test), esp32cam
  build SUCCESS, PlatformIO 6.2.0 provisioned via pip --user (no
  sudo needed — same pattern as B2B-Core's static PHP).
- Pending: physical bench session for the LED grammar + the full
  CAMERA_STATION flow; B2B-Core TASK-027 (same session) closed the
  backend punch list.

## TASK-011 delivery closed (2026-09-14, RUN-2026-09-14-firmware-014)

- The reader speaks HCE: `HceProtocol` (pure-C++ APDU core — builders
  byte-exact, parsers, vendored SHA-256/HMAC pinned by an RFC 4231
  vector) + `Rc522NfcReader` on `MFRC522Extended` (drop-in per the
  vendored 1.4.12 source: auto-RATS on SAK bit 6, `uid` member still
  filled). SAK+ATS → SELECT → fresh-nonce CHALLENGE → constant-time
  verify → application-level credId; MIFARE path behaviorally untouched.
- `NfcReader::lastKind()` (`physical` | `hce`) reaches pairing as
  `credential_kind`; tap/associate bodies unchanged (uid-only).
  `HCE_SECRET` in both secrets templates (+ `#ifndef` dev fallback).
- Docs: `HCE_PROTOCOL.md` + `.es.md` (canonical byte spec),
  API_INTEGRATION / PAIRING / MANUAL_VERIFICATION_CHECKLIST (both
  languages, incl. bench §10 with the 3-tap UID-independence check).
- Verification: native 111/111 (+15), esp32dev + esp32dev-mock +
  esp32cam builds SUCCESS, Android 3/3 + debug APK. Human bench §10
  pending (no hardware in agent runs).

## TASK-012 delivery (2026-09-15)

- New opt-in env `esp32cam-reader` (ADR-014): the reader image
  (`src/main.cpp`, HCE included) on an AI-Thinker ESP32-CAM board with
  no camera code/library. RC522 on the station's pins via the shared
  `config/esp32cam_board.h`; single GPIO33 LED feedback (`StationLed`);
  no buzzer. `scripts/flash.sh --cam-reader`.
- Verification: native 112/112, all four board envs build. Bench flash
  pending.
