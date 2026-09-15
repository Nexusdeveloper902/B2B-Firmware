# TASK-011 — Android HCE reader integration (phone-as-credential)

## Ask
Support the Pulse Android HCE phone alongside physical MIFARE cards on
the same RC522: detect ISO-DEP, run the SELECT AID + CHALLENGE exchange,
verify HMAC, and submit the application-level credential id — identified
by APDU, never by RF UID.

## Done
- `lib/PresenceCore/src/HceProtocol.{h,cpp}` — pure-C++ protocol core.
- `lib/NfcReader/src/Rc522NfcReader.h` — `MFRC522Extended` member, HCE
  branch in `poll()`, `lastKind()`, `HCE_SECRET` fallback.
- `lib/NfcReader/src/NfcReader.h` — `lastKind()` default `"physical"`.
- `lib/PresenceCore` `Mode`/`Modes`/`PayloadBuilder` — optional kind
  through pairing only (`credential_kind: hce`); tap wire unchanged.
- `src/main.cpp` + `src/camera/station.cpp` — pass `lastKind()` through.
- `include/secrets.h.example` + `secrets.camera.h.example` — `HCE_SECRET`.
- Tests: `test_hce_protocol.cpp` (11), payload +2, modes +2.
- Docs: `HCE_PROTOCOL.md` + `.es.md` (canonical), API_INTEGRATION /
  PAIRING / MANUAL_VERIFICATION_CHECKLIST updates (both languages).
- Android app integrated at `B2B-App/pulse-credential` (Pulse identity,
  BuildConfig `PULSE_*` seam, 3/3 unit tests, debug APK builds).

## Gates
`pio test -e native` 111/111 · `pio run -e esp32dev` SUCCESS ·
`pio run -e esp32dev-mock` SUCCESS · `pio run` (esp32cam) SUCCESS.

## Pending
Human bench §10 (real phone + board: §10.1–10.5 incl. the 3-tap
UID-independence observation). No hardware in agent runs.
