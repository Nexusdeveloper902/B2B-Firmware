# ADR-013: HCE phone support inside the existing reader (no parallel stack)

## Status
**Key model superseded by ADR-018 (2026-09-16):** no shared `HCE_SECRET`; the reader relays the proof and B2B-Core verifies with per-credential keys. The rest of this ADR (ISO-DEP branch, kind, UID-independence) stands.

Accepted (2026-09-14, TASK-011)

## Context
The standalone prototype proved SELECT AID + CHALLENGE against a phone,
but as a throwaway sketch: hardcoded secret, no backend, no MIFARE
coexistence, no tests beyond the happy path. The reader firmware already
has the three-way split (NfcReader / Mode / Feedback) and a host-tested
PresenceCore — the HCE path must live inside that split, not beside it.

## Decision
1. **APDU logic in PresenceCore (`HceProtocol`)**: builders, parsers and
   HMAC-SHA256 as pure C++ — host-tested including an RFC 4231 vector
   for the vendored SHA-256 (no mbedtls in the test env, no new
   dependency on device either). RF framing stays in the reader driver.
2. **`MFRC522Extended`, not a new driver**: the same library already
   ships ISO-DEP (`PICC_Select` override → RATS/ATS/PPS, `TCL_Transceive`
   I-blocks). Switching the member type is drop-in — the MIFARE path
   (`uid` member, HaltA, StopCrypto1) is byte-identical behavior.
3. **Branch on SAK bit 6 + ATS**: ISO-DEP target → SELECT → fresh
   `esp_random` nonce → constant-time verify → application-level credId;
   anything else → legacy UID path. Failures release the target and stay
   responsive; neither path can affect the other.
4. **Kind out, UID never in**: `NfcReader::lastKind()` (`"physical"` |
   `"hce"`) reaches pairing as `credential_kind`; the tap body is
   unchanged (uid-only). `HCE_SECRET` lives in the gitignored secrets
   files with a `#ifndef` dev-default fallback (MODE_PASSWORD precedent).
5. **Spec lives here**: `docs/HCE_PROTOCOL.md` (+`.es.md`) is canonical
   (wire bytes unchanged from the prototype); B2B-Core docs point at it.

## Consequences
Native 111/111 (15 new: 11 HCE protocol incl. UID-independence pin, 2
payload, 2 mode); `esp32dev` + `esp32dev-mock` + `esp32cam` all SUCCESS.
Bench §10 checklist written — human run with real phone + board pending.
Residual: single dev pre-shared key; per-credential keys, replay
protection, mutual auth recorded in the spec, not implemented.
