# TASK-001-reader-firmware-mvp

Build a basic, working ESP32 firmware for a Presence Platform NFC reader
with exactly two operating modes:

1. **Pairing Mode** — scan a card, associate its UID with a student in the
   backend database, via the pairing endpoint built in `b2b-core` as its own
   TASK-010 (Phase E1 of this task; see the master protocol Section 0.3).
2. **Operation Mode** — scan a card, log a presence-event tap against the
   backend, show feedback.

PlatformIO project for ESP32 (Arduino framework, esp32dev). Bilingual
(EN/ES) real documentation in `docs/` — not only `.agent/`. Intentionally
minimal otherwise: no camera/recycling flow, no OTA, no captive-portal
provisioning. Get the core loop of both modes solid before anything else.

Phases: A (scaffolding) → B (NFC abstraction + mock) → C (mode system) →
D (operation mode) → E1 (b2b-core pairing endpoint, tracked there) →
E2 (pairing mode firmware) → F (resilience & feedback) → G (tests & docs).

---

## Commit — (pending)
Date: 2026-09-05
Branch: feature/TASK-001-reader-firmware-mvp (genesis commit on main)
Phase: A

Status: IN PROGRESS — commit documentation appended per phase below as
commits land (append-only; see the RUN record for the final state).

## Acceptance criteria

```text
Phase A
[ ] PlatformIO project scaffolded, compiles for esp32dev target
[ ] secrets.h.example committed, secrets.h gitignored
[ ] config.h documents RC522 pin assignments with a wiring-confirmation note
[ ] Wi-Fi connects on boot with basic reconnect handling

Phase B
[ ] NfcReader interface defined; RC522 implementation present
[ ] MockSerialNfcReader implemented and usable for testing without hardware

Phase C
[ ] Mode determined at boot from a documented pin/button state
[ ] Status LED continuously indicates current mode
[ ] Mode logic implemented as swappable strategy classes

Phase D
[ ] Operation-mode tap call implemented against the CONFIRMED b2b-core contract
[ ] Distinct feedback for success / not-recognized / auth-failure / network-failure
[ ] Device recovers and remains responsive after any failure case

Phase E1 (in b2b-core, its own TASK-010)
[ ] b2b-core's next task number determined by inspection, not guessed (TASK-010)
[ ] pending_pairings migration created
[ ] POST /api/v1/admin/students/{id}/arm-pairing implemented per spec
[ ] POST /api/v1/admin/cards/pair implemented per spec, incl. duplicate-card
    and no-active-session rejection
[ ] Feature tests cover: happy path, no active session, already-paired card,
    expired session
[ ] docs/API.md (+.es.md) and Postman collection updated in b2b-core
[ ] ADR recorded in b2b-core
[ ] b2b-core's own main verified independently (tests + migrate) after merge

Phase E2 (firmware)
[ ] Pairing-mode HTTP call implemented against the now-live endpoint
[ ] Distinct feedback for pairing success / no-active-session /
    already-paired / network-failure
[ ] End-to-end design tested via mock reader + armed session instructions
    (real end-to-end hardware verification → checklist)

Phase F
[ ] Card-read debouncing implemented
[ ] Main loop uses non-blocking timing
[ ] FeedbackController interface separates LED/buzzer logic from business logic

Phase G
[ ] Host-side unit tests exist and pass
[ ] docs/HARDWARE_SETUP.md (+.es.md) complete
[ ] docs/MANUAL_VERIFICATION_CHECKLIST.md (+.es.md) complete, covering every
    scenario listed in the task spec
```

## Confirmed backend contract (verified in b2b-core main, not assumed)

- Tap: `POST /api/v1/events/tap`, `Authorization: Bearer <reader.api_key>`,
  body `{"credential_uid": "...", "client_timestamp": "..." (optional)}`.
  200 → `{status, event_id, event_type, student_first_name, next_step}`;
  404 → unknown/inactive card; 401 → bad key; 422 → validation.
  (routes/api.php + TapEventController, verified 2026-09-05.)
- Pairing (after b2b-core TASK-010): `POST /api/v1/admin/cards/pair` with
  the same Bearer reader key, body `{"credential_uid": "..."}`.
  200 → `{status, paired_student_name, student_id}`; 409 → no active
  session; 422 → card already paired; 401 → bad key.
  Arming: `POST /api/v1/admin/students/{id}/arm-pairing` (admin session /
  PAT) → `{status, student_id, expires_at}`.
