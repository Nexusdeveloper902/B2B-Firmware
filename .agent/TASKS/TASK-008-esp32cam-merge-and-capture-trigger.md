# TASK-008-esp32cam-merge-and-capture-trigger

Status: OPEN (not started)
Created by: RUN-2026-09-07-firmware-008 (owner-directed audit of
2026-09-07; see B2B-Core TASK-025 for the backend half).

## Objective

Fulfill the owner's recycling spec §6/§7/§8/§37 for THIS repository:

1. Merge the working ESP32-CAM image-acquisition firmware from the
   reference repo ESP32-CAM-CV/firmware/esp32_cam into this repo
   (camera init, JPEG capture, HTTP serving of captured images).
   Preserve the reader functionality untouched (RC522, modes,
   pairing, bearer auth — TASK-001..007).
2. Add a PlatformIO environment for the camera target based on the
   REFERENCE platformio.ini (board esp32cam, espressif/esp32-camera
   lib, monitor_dtr=0, monitor_rts=0). Do NOT invent a new board
   config; the reference is known-working (its build verified green
   on 2026-09-07 by the audit run).
3. Add the terminal-triggered capture: pressing ENTER over serial
   triggers a capture request (the temporary physical trigger; the
   future IR sensor must be a drop-in replacement at the trigger
   layer only). Requirements from the spec:
   - Enter must not multi-fire on key-repeat/buffered input (the
     existing LineBuffer in the mode console already solves this
     class of problem — reuse the pattern).
   - A capture has a clear success/failure state.
   - ENTER never triggers DeepSeek/classification by itself — the
     backend owns that decision (spec §4/§8).
4. Wire the captured image to the backend: POST
   /api/v1/recycling/classify (multipart: event_id + image) with the
   same Bearer reader auth as tap. The event_id comes from the tap
   that preceded the capture (card-first flow; the bottle-first flow
   is backend work — B2B-Core TASK-025 item 2).
5. Keep the trigger replaceable: a CaptureTrigger boundary
   (TerminalTrigger now, IRTrigger later) per spec §37.

## Acceptance criteria

- `pio run` succeeds for ALL environments (reader + camera targets).
- `pio test -e native` stays green (existing 68 + new tests for the
  trigger abstraction and capture payload building).
- A documented serial ENTER → capture → upload flow that works
  against a running B2B-Core (manual bench verification is the
  owner's; provide the exact monitor commands and expected output).
- Fresh checkout compiles without secrets.h in every env (the
  __has_include pattern from TASK-001 applied to the merged camera
  sources too — the reference repo currently FAILS fresh checkout).

## Out of scope

- IR sensor hardware integration (spec §2 postpones it).
- DeepSeek/API logic (backend owns it — never in firmware).
- Bottle-first association (backend — B2B-Core TASK-025).

## Resolution (append 2026-09-07, RUN-2026-09-07-firmware-010)

Status: COMPLETED — all acceptance criteria met, merged to main (commit
cfdfa3c). Evidence: native 86/86 (68 + 18 new tests); pio run esp32dev
+ esp32dev-mock + esp32cam ALL SUCCESS; fresh checkout without secrets.h
compiles (the __has_include guard covers the camera build — warning
path exercised). Refinement over the original objective 4 wording (see
ADR-008): the primary upload is the bottle-first POST /recycling/capture
(landed in B2B-Core TASK-025 the same date — the camera needs no
event_id); card-first classify is still wired via 'e <event_id>'. The
documented bench script (docs/CAMERA_STATION.md + .es.md) is the
owner's manual verification, per the repo's hardware-honesty convention.
