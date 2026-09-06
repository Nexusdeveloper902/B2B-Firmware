# ADR-008

## Date
2026-09-07

## Context
TASK-008 (owner's recycling spec §6/§7/§8/§37) requires merging the
ESP32-CAM reference firmware into this repo and wiring captured images
to B2B-Core. The task text (written 2026-09-07 before the backend's
bottle-first endpoints existed) specified POST /api/v1/recycling/
classify with an event_id "from the tap that preceded the capture". The
same date, B2B-Core TASK-025 landed POST /api/v1/recycling/capture
(image WITHOUT any student, held awaiting_card) and
POST /api/v1/recycling/captures/{id}/associate (the card that resolves
it) — a strictly better contract for a standalone camera that knows
nothing about taps. Spec §37 also demands the trigger stay replaceable
(ENTER today, IR sensor later).

## Decision
1. The camera station's PRIMARY upload is the bottle-first endpoint:
   ENTER → capture → POST /api/v1/recycling/capture. The station needs
   zero knowledge of tap events, and the spec §4 cost gate is enforced
   server-side (no classifier call until a card resolves the capture).
2. Card-first classify is ALSO wired, opt-in: the operator arms an
   event_id ('e <event_id>') and the next ENTER captures and POSTs
   /api/v1/recycling/classify — the TASK-008-lettered flow, still fully
   supported.
3. The resolution call ('a <credential_uid>' → associate) rides the
   reader's own EspApiClient — one Bearer auth path for the whole repo.
4. The physical trigger lives behind the CaptureTrigger interface
   (TerminalCaptureTrigger today; the future IR sensor is a drop-in at
   that seam ONLY — spec §37). ENTER cannot multi-fire: line discipline
   plus a 2 s cooldown with an injectable clock, host-tested.

## Alternatives Considered
- classify-only wiring (the task's original text) — rejected as the
  primary: a standalone camera cannot know event_id; would need a new
  backend endpoint to fetch the awaiting event, or operator bookkeeping.
- A combined camera+RC522 single-device firmware — rejected: preserves
  neither the reference's verified camera code nor the untouched-reader
  requirement; the ESP32-CAM board's free GPIO would force soft-SPI.
- Camera polling the backend for pending events — rejected: more
  device state, more failure modes, zero benefit over bottle-first.

## Reasoning
The backend evolution (TASK-025, same date) made the tap-less contract
possible; using it removes the only unsolved coupling in the original
design. Both spec §3 cases remain demonstrable from the camera station
alone, and every wire byte + the trigger semantics are host-testable.

## Consequences
- The camera station is a READER identity to the backend (Bearer key of
  a recycling readers row) — provisioning is documented; the demo seed
  supports sharing the single recycling reader key across devices.
- Associate requires the same reader identity that stored the capture —
  by design (403 otherwise), which the shared-key mode satisfies.
- When the IR sensor arrives, it replaces TerminalCaptureTrigger only.

## Status
ACTIVE
