# Pulse HCE Credential Protocol (v1)

> También disponible en: [Español](HCE_PROTOCOL.es.md)
> Canonical byte-level reference for the phone-as-credential path.
> Phone side: `B2B-App/pulse-credential` (`ApduProtocol`).
> Reader side: `lib/PresenceCore` (`HceProtocol`) + `lib/NfcReader`
> (`Rc522NfcReader` HCE branch). Backend contract: B2B-Core
> `docs/API.md` §“Android HCE credentials”. Adapted from the verified
> standalone prototype (`RC522-Android/`, `PROTOCOL.md` there) — the
> wire bytes are unchanged; what changed is the integration around them
> (credential kind, Pulse endpoints, desk UI).

Phone-as-credential over NFC for ESP32 + RC522. Deliberately tiny: two APDUs.

```text
NFC-A
  ↓  anticollision + SELECT (SAK bit 6 set => T=CL supported)
ISO-DEP (ISO/IEC 14443-4)
  ↓  RATS / ATS (+ PPS), I-blocks via MFRC522Extended::TCL_Transceive
APDU
  ↓  SELECT AID -> CHALLENGE -> verify
Pulse credential protocol (application-level identity)
  ↓  credential_uid + credential_kind=hce → B2B-Core
```

No MIFARE Classic anywhere on this path: Android HCE cannot emulate it,
so the phone presents an ISO-DEP target instead. Physical cards keep
their own untouched path (RF UID → `credential_kind` omitted).

## RF layer

| Layer | Detail |
|---|---|
| RF | NFC-A, 106 kbit/s (PPS negotiates ≤ 212 kbit/s only if the phone offers it) |
| Anticollision | Standard cascade SELECT; SAK `0x20` = ISO-DEP (HCE) target |
| Activation | RATS (`E0 50` + CRC_A, FSD = 64, CID = 0) → ATS; PPS only if ATS/TA1 present |
| Framing | ISO-DEP I-blocks, CID = 0, no NAD; phone-to-reader chaining ACKed by the reader |
| Limits | RC522 FIFO = 64 B, so one I-block carries ≤ ~57 B of APDU payload |
| Driver | `MFRC522Extended` (same `miguelbalboa/MFRC522` library, no extra dependency): `PICC_Select()` override does anticollision + SELECT + RATS/ATS + PPS when SAK bit 6 signals T=CL; `TCL_Transceive(tag, apdu, len, resp, &respLen)` does I-blocks with block-number toggle, CID handling, and chained-response ACKs. Known limits (documented, not worked around): FSD fixed at 64, CID hardcoded to 0, no NAD, no S-block WTX (the phone answers in ms — HCE does), bitrates above 212 kbit/s not attempted. |

## Application layer

- Pulse AID: **`F0010203040506`** (7 bytes, proprietary `F…` category —
  valid, consistent on both sides, collides with nothing production).
- Credential id: variable-length ASCII (prototype: `TEST-ANDROID-001`,
  16 bytes, public, not secret). The reader accepts 1–32 bytes.
- **The phone's NFC UID is random per tap on Android HCE** — the reader
  logs only its length (discovery proof) and NEVER uses it as identity.
  The backend never receives it: there is no `rf_uid` column, no `rf_uid`
  input, and tap lookup is `credential_uid`-only.

## APDU 1 — SELECT AID

Request (12 bytes, `Le` omitted):

```text
CLA | INS | P1 | P2 | Lc | AID (7 bytes)
 00 | A4  | 04 | 00 | 07 | F0 01 02 03 04 05 06
```

i.e. `00 A4 04 00 07 F0010203040506`. A trailing `Le = 00` byte is accepted
and ignored.

Response, AID matched:

```text
48 43 45 2D 4F 4B ("HCE-OK") | 90 00
```

Response, AID not matched: `6A 82` (file/app not found). Session becomes
"selected" only on match; it resets on RF deactivation.

## APDU 2 — CHALLENGE (challenge-response)

Request (13 bytes, `Le` omitted; trailing `Le = 00` accepted):

```text
CLA | INS | P1 | P2 | Lc | nonce (8 bytes, fresh esp_random per tap)
 80 | 10  | 00 | 00 | 08 | NN NN NN NN NN NN NN NN
```

Response:

```text
credLen (1) | credId (1-32, ASCII) | HMAC-SHA256 (32) | 90 00
MAC = HMAC-SHA256(key = HCE_SECRET, msg = credId || nonce)
```

The reader recomputes the MAC over the credential id it received plus the
nonce it sent, and compares in constant time. Secrets are never logged
(only the per-tap nonce and the public id); a mismatch fails closed.

## Pulse integration (what the prototype did not have)

| Step | Behavior |
|---|---|
| Reader → backend (pair) | `POST /api/v1/admin/cards/pair` with `{"credential_uid": "<credId>", "credential_kind": "hce"}` — same arm-then-pair flow, same 409/422 semantics as physical cards |
| Reader → backend (tap) | `POST /api/v1/events/tap` with `{"credential_uid": "<credId>"}` — kind intentionally NOT sent (lookup is uid-only); phone taps resolve `credential → student → attendance / PAE / recycling` through the unchanged event spine |
| Backend storage | `cards.kind` (`physical` \| `hce`, default `physical`) — display/audit metadata; old firmware omitting the kind pairs exactly as before |
| Desk UI | Phone credentials badged (“Phone” / “Teléfono”) in roster chips, recent-pairings history (SSR + live WS rows) and the students desk |
| Key provisioning | `HCE_SECRET` in gitignored `secrets.h` / `secrets.camera.h` (templates document it); MUST match the Android app's secret or every phone tap fails closed. A secrets file predating HCE builds against the development-only default with a `#warning` (same precedent as `MODE_PASSWORD`) |

## Status words

| SW | Meaning | When |
|---|---|---|
| `9000` | Success | SELECT matched / CHALLENGE answered |
| `6A82` | App not found | SELECT with any other AID |
| `6985` | Conditions not satisfied | CHALLENGE before SELECT |
| `6700` | Wrong length | `Lc` ≠ 7 (SELECT) / ≠ 8 (CHALLENGE), truncated APDU |
| `6D00` | INS unknown | Known CLA, unknown instruction |
| `6E00` | CLA unknown | First byte not `00`/`80` |

## Maximum lengths

| Item | Max |
|---|---|
| SELECT request | 13 bytes (with optional Le) |
| SELECT response | 8 bytes |
| CHALLENGE request | 14 bytes (with optional Le) |
| CHALLENGE response | up to 67 bytes (1 + 32 + 32 + 2; prototype uses 51) |
| Single I-block INF | ~57 B — the 51-byte prototype response fits; a 32-byte credential id needs reader-side chaining ACKs (already implemented in `TCL_Transceive`) |

## Error conditions (reader diagnostics)

Driver caveat (found on the bench): MFRC522 1.4.12's `PICC_Select`
parses a good ATS into a stack local and never stores it in `tag` — so
`tag.ats.size` is always 0 through the driver path and any gate on it
is dead code (this bit the standalone prototype too). Deeper bench
fact: once this phone's controller is HaltA'd it answers nothing
(RATS, WUPA, even REQA) until field re-entry — so the driver's
select (immediate RATS + HaltA-on-timeout) poisons the tap, and every
post-halt retry is aimed at a corpse. Hence park-and-activate:
discovery uses the qualified BASE select (no RATS, no surprise HaltA —
identical frames to the override for non-ISO-DEP targets), an ISO-DEP
sighting parks the untouched selected session for 400 ms of radio
silence, and only then does the tap's FIRST RATS go out (`adoptAts`,
status logged, ATS adopted into `tag`). A target that won't answer
that RATS falls through to the UID path. No PPS is negotiated —
106 kbit/s is mandatory and plenty for <70 B exchanges.

`HCE SELECT failed` (no I-block
reply) · `HCE SELECT rejected (unknown AID)` (`6A82`) · `HCE CHALLENGE
timeout` · `HCE authentication FAILED (malformed or HMAC mismatch)`.
Every failure releases the target (`TCL_Deselect` + `PICC_HaltA`) and the
device stays responsive; MIFARE taps are never affected (that path never
runs for ISO-DEP targets, and vice versa).

## UID-independence evidence

1. **Unit** (`test_hce_protocol.cpp::test_hce_identity_ignores_the_rf_uid`):
   the parsers do not even take an RF UID — identical APDU bytes always
   yield the identical credential id.
2. **Backend** (`HceCredentialTest::identification_does_not_depend_on_the_rf_uid`):
   no `rf_uid` column exists; a non-hex credential id (impossible as an
   RF UID) pairs and taps end-to-end, twice, to the same student.
3. **Bench** (checklist §10): log the RF UID length across taps (it
   changes), verify the same credential id authenticates every time.

## Production hardening (explicitly out of scope)

Single development pre-shared key → per-credential keys from a secure
backend; add replay protection (reader-tracked challenge store /
monotonic counter); mutual authentication + encrypted channel (e.g.
SCP03-style); key rotation; side-channel review. The reader's Bearer key
vouches for the verified credential to the backend — the same trust a
physical UID gets. This spec proves the NFC path and the Pulse
integration, not a credential system.
