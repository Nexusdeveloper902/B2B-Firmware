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
