# Pulse HCE Credential Protocol (v1.1 — per-credential keys)

> También disponible en: [Español](HCE_PROTOCOL.es.md)
> Canonical byte-level reference for the phone-as-credential path.
> Phone side: `B2B-App/pulse-credential` (`ApduProtocol`).
> Reader side: `lib/PresenceCore` (`HceProtocol`) + `lib/NfcReader`
> (`Rc522NfcReader` HCE branch). Backend contract: B2B-Core
> `docs/API.md` §“Android HCE credentials”. Adapted from the verified
> standalone prototype (`RC522-Android/`, `PROTOCOL.md` there) — the
> wire bytes are unchanged; what changed is the integration around them
> (credential kind, Pulse endpoints, desk UI).

Phone-as-credential over NFC for ESP32 + RC522. Deliberately tiny: two APDUs
per tap, plus a third, one-time APDU (ENROLL) at pairing.

> **v1.1 (TASK-015, ADR-018; B2B-Core ADR-068; B2B-App ADR-004).** The
> SELECT and CHALLENGE bytes are **unchanged**. What changed is the key:
> there is **no shared `HCE_SECRET` anywhere** any more. Every phone
> credential has its own 256-bit key, stored in that phone's Android
> Keystore; B2B-Core holds the only other copy, encrypted at rest. The
> **reader holds no HCE key and verifies nothing**: it relays its nonce
> and the phone's MAC, and **B2B-Core verifies** them with that
> credential's key. Additive changes are the `ENROLL` APDU (pairing
> only) and the `6A88` status word (no key on the phone).

```text
NFC-A
  ↓  anticollision + SELECT (SAK bit 6 set => T=CL supported)
ISO-DEP (ISO/IEC 14443-4)
  ↓  RATS / ATS (+ PPS), I-blocks via MFRC522Extended::TCL_Transceive
APDU
  ↓  SELECT AID -> CHALLENGE [-> ENROLL, pairing only]
Pulse credential protocol (application-level identity)
  ↓  credential_uid + hce_nonce + hce_mac (+ wrapped key at pairing) → B2B-Core
B2B-Core
  ↓  HMAC-SHA256(K_cred, credId || nonce) == hce_mac ? (constant time, nonce single-use)
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
MAC = HMAC-SHA256(key = K_cred, msg = credId || nonce)
```

`K_cred` is **this credential's own key** (32 bytes, Android Keystore on
the phone). A phone that has no usable key answers **`6A88`** instead of
a MAC (not linked yet, or the Keystore was wiped by a reinstall).

The reader does **not** verify the MAC — it has no key. It sends the
credential id, its own nonce (`hce_nonce`, 16 hex) and the phone's MAC
(`hce_mac`, 64 hex) to B2B-Core, which recomputes the MAC with that
credential's stored key, compares in constant time, and accepts each
nonce once per credential. The nonce and MAC are safe to log; key
material never is. Any mismatch fails closed on the backend (`403`).

## APDU 3 — ENROLL (pairing only, one-time key hand-off)

Sent by a reader in **PAIRING mode only**, right after CHALLENGE in the
same session. Request (5 bytes; the 4-byte header without `Le` is also
accepted; a data field is `6700`):

```text
CLA | INS | P1 | P2 | Le
 80 | 20  | 00 | 00 | 20
```

Response, only while the holder has opened the phone's **enrollment
window** ("Link this phone", 60 s), and **only once** per window:

```text
K_cred (32) | 90 00          (34 bytes: fits one RC522 I-block)
```

Otherwise `6985` (window closed, expired, already released, or not
SELECTed). Opening the window first **replaces** the phone's key in the
Keystore, so a copy read by anyone else becomes useless the moment the
holder retries.

The reader never sends `K_cred` in clear over Wi-Fi. It wraps it with a
fresh 16-byte nonce and its own API key:

```text
hce_key_nonce   = 32 hex chars (esp_random, fresh per pairing)
pad             = HMAC-SHA256(READER_API_KEY,
                    "pulse-hce-key-wrap/v1\n" || credId || "\n" || hce_key_nonce)
hce_key_wrapped = hex(K_cred XOR pad)
```

The pair request is also Pulse-HMAC signed (ADR-016: body-bound,
single-use nonce). B2B-Core unwraps the key, requires the same tap's
CHALLENGE proof to verify under it (proof of possession), then stores it
encrypted. The raw key is wiped from the reader's RAM right after
wrapping and is never logged (the retry path redacts ENROLL frames).

## Shared test vector (pinned in all three repos)

| Item | Value |
|---|---|
| `K_cred` | `000102…1f` (bytes 0..31) |
| `credId` | `PLS-K3Y7V3CT0R5Z` |
| `nonce` | `0123456789abcdef` |
| `MAC` | `ba6d0fbf5106f79257727819d19a17b7e15abc4b8a0d1bfe71cd76090488a295` |
| CHALLENGE response | `10` `504c532d4b335937563343543052355a` `ba6d…a295` `9000` |
| `READER_API_KEY` | `test-secret-000000000000000001` |
| `hce_key_nonce` | `000102030405060708090a0b0c0d0e0f` |
| `hce_key_wrapped` | `c5bb9fe15dff66a8636366dd4e73e0a3146601e3b3c36472961de142a9a914c2` |

Computed outside all three implementations (Python `hmac`); pinned in
`test/test_hce_protocol.cpp` here, `HceCredentialAuthTest` in B2B-Core,
and `ApduProtocolTest` in B2B-App. Each suite also pins RFC 4231
known-answer HMAC vectors.

## Pulse integration (what the prototype did not have)

| Step | Behavior |
|---|---|
| Reader → backend (pair) | `POST /api/v1/admin/cards/pair` with `{"credential_uid", "credential_kind": "hce", "hce_nonce", "hce_mac", "hce_key_nonce", "hce_key_wrapped"}` — same arm-then-pair flow and 409/422 semantics as physical cards; `403` = proof did not verify under the handed-over key; an active phone of the **same** armed student is re-keyed (`"rekeyed": true`) |
| Reader → backend (tap) | `POST /api/v1/events/tap` with `{"credential_uid", "hce_nonce", "hce_mac"}` — kind intentionally NOT sent (it is server-side); a phone card without a valid, fresh proof answers `403 hce_auth_failed`. Same fields on `/recycling/captures/{id}/associate` |
| Backend storage | `cards.kind` (`physical` \| `hce`) + `hce_credential_keys` (one encrypted key per phone card); revocation (`POST /admin/cards/{id}/revoke`) destroys the key |
| Desk UI | Phone credentials badged (“Phone” / “Teléfono”) in roster chips, recent-pairings history (SSR + live WS rows) and the students desk |
| Key provisioning | Per credential, at pairing: the holder opens "Link this phone", the reader (PAIRING mode) sends ENROLL and wraps the key under `READER_API_KEY`. **No HCE key exists in any secrets file, build flag or source.** An old secrets file that still defines `HCE_SECRET` is ignored |

## Status words

| SW | Meaning | When |
|---|---|---|
| `9000` | Success | SELECT matched / CHALLENGE answered |
| `6A82` | App not found | SELECT with any other AID |
| `6985` | Conditions not satisfied | CHALLENGE/ENROLL before SELECT; ENROLL with no open enrollment window |
| `6A88` | Referenced data not found | CHALLENGE on a phone with no usable key (not linked / reinstalled) |
| `6700` | Wrong length | `Lc` ≠ 7 (SELECT) / ≠ 8 (CHALLENGE), ENROLL with data, truncated APDU |
| `6D00` | INS unknown | Known CLA, unknown instruction |
| `6E00` | CLA unknown | First byte not `00`/`80` |

## Maximum lengths

| Item | Max |
|---|---|
| SELECT request | 13 bytes (with optional Le) |
| SELECT response | 8 bytes |
| CHALLENGE request | 14 bytes (with optional Le) |
| CHALLENGE response | up to 67 bytes (1 + 32 + 32 + 2; `PLS-` ids use 51) |
| ENROLL request / response | 5 bytes / 34 bytes |
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
timeout` · `HCE phone has NO key yet (6A88)` · `HCE CHALLENGE answer
malformed` · `HCE ENROLL refused` (pairing: the phone's window is not
open). A wrong key is no longer a reader-side error: the tap reaches the
backend and answers `403` (`[404]`-style rejection on the serial).
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
   changes), verify the same credential id is accepted every time.

## Security boundaries (what v1.1 does and does not guarantee)

**Guaranteed now**
- No shared HCE secret: dumping one APK, one reader or one secrets file
  yields no key that works for any other credential.
- A phone key authenticates only its own credential id (the id is inside
  the MAC, and the backend looks the key up by that id).
- Readers cannot forge phone taps: a valid reader signature without a
  valid, fresh phone proof is refused (`403`).
- Replayed transcripts are refused (per-credential nonce memory, 7 days).
- Revocation (lost/stolen phone) destroys the backend key; the phone's
  key is then worthless even if the card status were flipped back.
- Provisioning cannot overwrite another student's credential: only an
  active phone of the student the admin armed can be re-keyed.

**Still true (documented limits, not solved here)**
- Unilateral authentication, reader-chosen nonce: someone holding a
  reader key who skims a phone can present that one transcript once
  (pre-play) before the phone's next legitimate tap. The backend
  refuses it after first use.
- The key crosses NFC in clear **once**, during the holder-opened,
  single-release enrollment window at the desk. An eavesdropper at that
  exact moment could copy it. Mitigation: the window is short and
  single-use, and re-linking rotates the key.
- The backend is the trust root: its database plus `APP_KEY` expose
  every phone key (symmetric scheme). An asymmetric (ECDSA) credential
  would remove this, at the cost of a new CHALLENGE format beyond one
  RC522 I-block.
- The phone answers while locked (policy: `requireDeviceUnlock=false`,
  no user-auth-bound key). A stolen phone taps until it is revoked.
- Physical MIFARE UIDs still have no cryptography (clonable).
- Mutual authentication, an encrypted NFC channel and a side-channel
  review remain out of scope.
