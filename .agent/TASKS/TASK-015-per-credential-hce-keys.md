# TASK-015 — per-credential HCE keys (remove the shared HCE_SECRET)

Decision: ADR-018. Companion work: B2B-Core TASK-049 (ADR-068, the
verifier, key storage, revocation) and B2B-App TASK-003 (ADR-004, the
Keystore key and enrollment window).

## Ask
Remediate the CRITICAL audit finding: one HMAC secret
(`dev-only-prototype-secret-001`) was compiled into every APK and every
reader, so one dump could impersonate any phone credential. Replace it
with per-credential keys, keep the HCE wire protocol, add server-side
revocation, and prove it with independent known-answer vectors.

## Firmware (this repo) — DONE
- `PresenceCore/HceProtocol`:
  - added `INS_ENROLL`, `buildEnroll`, `parseEnrollResponse`,
    `isKeyMissing` (`6A88`), and `wrapKeyHex`;
  - removed `verifyChallengeResponse`.
- `CoreTypes`:
  - new `HceProof` (nonce, MAC, wrapped key, key nonce);
  - new `PairOutcome::ProofRejected`;
  - `ResponseParser` maps `403` (tap → `CardNotRecognized`,
    pair → `ProofRejected`).
- `PayloadBuilder`: tap, pair and associate bodies take an optional proof.
  Physical bodies stay byte-identical, and a tap never carries key
  fields.
- `Mode` / `Modes`: `onCardTap(uid, kind, proof)`.
- `NfcReader`: `lastProof()`, `setEnrollment(on, wrapSecret)`.
- `Rc522NfcReader`:
  - relays the proof instead of verifying;
  - ENROLL plus wrap in pairing, then wipes the key;
  - ENROLL frames are redacted in the salvage logs;
  - the `HCE_SECRET` fallback is removed.
- `src/main.cpp`, `src/camera/station.*`: forward the proof; the mode
  switch drives enrollment; associate relays the proof too.
- `include/*.example` and the local `secrets.cam_reader.h`: `HCE_SECRET`
  removed.
- Build `hce.18` → `hce.19`.
- Docs (EN + ES):
  - `HCE_PROTOCOL` v1.1: ENROLL, `6A88`, the wrap, the shared vector,
    and the security boundaries;
  - `PAIRING`: the phone flow and outcomes;
  - `MANUAL_VERIFICATION_CHECKLIST` §10, rewritten as 10.1–10.8.
- Tests:
  - `test_hce_protocol.cpp`: the shared-vector response literal, the MAC
    literal, the wrap literal, ENROLL builder and parser, `6A88`, the
    malformed cases, and wrong key, nonce and credential;
  - payload, mode and response tests for the proof and `403`.

## Not done here (hardware)
- Bench checklist §10.2–10.8 needs a real phone and board, so it was not
  run.
- The RF timing of the Keystore HMAC and the 3-APDU pairing tap is
  unmeasured.
