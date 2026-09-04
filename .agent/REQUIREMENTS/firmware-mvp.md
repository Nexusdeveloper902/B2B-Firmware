# Firmware MVP requirements (TASK-001 scope)

## Source
Master execution protocol RUN-2026-09-03-firmware-001 (owner-supplied
task specification, Sections 0–25). This file restates the binding
requirements for the firmware repository only; the b2b-core pairing
endpoint requirements are tracked in b2b-core's own TASK-010.

## Functional requirements

FR-1 The firmware is a PlatformIO project for ESP32 (esp32dev board,
     Arduino framework).
FR-2 Two operating modes:
     FR-2a OPERATION MODE: on a scanned card UID, POST the tap to the
          backend's tap-ingestion endpoint (contract verified in
          b2b-core routes/api.php: POST /api/v1/events/tap) with
          Authorization: Bearer <READER_API_KEY>.
     FR-2b PAIRING MODE: on a scanned card UID, POST to
          POST /api/v1/admin/cards/pair (same Bearer identity pattern)
          once the endpoint exists in b2b-core main (Phase E1 → E2).
FR-3 Mode is selected at boot from the mode-select pin state; the mode LED
     indicates the mode CONTINUOUSLY (not just at boot).
FR-4 Distinct feedback (LED patterns + serial log, EN/ES) for: success,
     card-not-recognized (404), no-active-pairing-session (409),
     already-paired (422), auth-failure (401), network/timeout failure.
     The device must recover and stay responsive after any failure.
FR-5 Card reads are debounced (same-UID cooldown).
FR-6 Main loop is non-blocking (millis()-based; no delay() in loop()).
FR-7 Wi-Fi connects on boot with bounded timeout and periodic background
     reconnect; never hangs, never crashes.
FR-8 A serial-simulated mock reader allows virtual taps by typing a UID in
     the Serial Monitor, for hardware-less development.

## Non-functional requirements

NFR-1 Real credentials (Wi-Fi, API base URL, reader API key) live ONLY in
      the gitignored include/secrets.h; the committed template is
      include/secrets.h.example. No secret is ever committed, logged in
      run reports, or printed.
NFR-2 Documentation is bilingual (English + Spanish) in REAL docs/
      (README.md + README.es.md, docs/HARDWARE_SETUP.md + .es.md,
      docs/API_INTEGRATION.md + .es.md, docs/MANUAL_VERIFICATION_CHECKLIST.md
      + .es.md), not only .agent/.
NFR-3 NFC reader driver, mode logic, and feedback logic are separately
      swappable (three-way interface split — see ARCHITECTURE/).
NFR-4 Honesty: verification claims are bounded to compilation + host tests
      + mock behavior; hardware-in-the-loop verification is a human task
      via the checklist.

## Out of scope
Camera/recycling-classification flow; remote reader-mode reconfiguration;
Wi-Fi captive-portal provisioning; OTA updates; multi-reader images;
any b2b-core change beyond the TASK-010 pairing endpoint.
