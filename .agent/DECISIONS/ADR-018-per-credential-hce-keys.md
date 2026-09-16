# ADR-018: per-credential HCE keys — the reader relays, B2B-Core verifies

## Status
Accepted (2026-09-16, TASK-015). Supersedes the key model of ADR-013
(§3 "constant-time verify" and §4 "`HCE_SECRET` in the secrets files").
Wire bytes of SELECT/CHALLENGE unchanged.

## Context
The engineering audit (CRITICAL; red-team RT-006) found one shared HMAC
secret (`dev-only-prototype-secret-001`) compiled into every phone APK
and every reader's secrets file. A single APK or reader dump let anyone
answer a CHALLENGE for any credential id. Worse, B2B-Core never saw any
proof: it trusted the reader's claim that a phone was verified, so a
reader key alone could forge a phone tap.

Per-credential keys make local verification impossible without shipping
every phone's key to every reader, which would turn each reader into a
key vault. The APDU already authenticates `credId || nonce`, so the key
can be looked up per credential. What had to change was who verifies.

## Decision
1. **The reader holds no HCE key and verifies nothing.** After
   CHALLENGE it relays `hce_nonce` (its own nonce, 16 hex) and
   `hce_mac` (the phone's MAC, 64 hex) in the tap, pair and associate
   bodies (`HceProof`, `PayloadBuilder`). B2B-Core verifies with that
   credential's key (B2B-Core ADR-068). `Hce::verifyChallengeResponse`
   and the `HCE_SECRET` fallback are deleted, and `HCE_SECRET` is removed
   from every secrets template.
2. **ENROLL `80 20 00 00 20` (PAIRING mode only)** fetches the phone's
   32-byte key right after CHALLENGE. The phone answers only inside its
   holder-opened, single-release window (B2B-App ADR-004); otherwise it
   answers `6985`. `NfcReader::setEnrollment(on, wrapSecret)` is driven
   by the mode switch, so OPERATION taps never ask for a key.
3. **The key never crosses Wi-Fi in clear.** `Hce::wrapKeyHex` computes
   `K XOR HMAC-SHA256(READER_API_KEY, "pulse-hce-key-wrap/v1\n" ||
   credId || "\n" || keyNonce)` with a fresh 16-byte `esp_random` nonce.
   The request is already Pulse-HMAC signed (ADR-016). The raw key is
   wiped at once, and ENROLL frames are redacted from the retry and
   salvage diagnostics.
4. **`6A88` = the phone has no key.** It is logged with the fix ("link
   the phone"), and no backend call is made. A `403` from the backend
   (proof refused) maps to the existing rejection feedback
   (`CardNotRecognized` / `PairOutcome::ProofRejected` →
   `PairAlreadyPaired` LED).
5. **One shared vector.** The key, credential, nonce, MAC, reader key,
   wrap nonce and wrapped value are pinned literally here, in B2B-Core
   and in B2B-App.

## Alternatives rejected
- **Sync all phone keys to readers:** every reader becomes a key vault,
  and revocation needs a push channel.
- **ECDSA in the phone Keystore (non-exportable):** the strongest key
  custody, but CHALLENGE would grow past one RC522 I-block (a bench
  risk) and it needs a new verifier path. Recorded as the successor.
- **Fetch the key from the backend over the phone's network link:** it
  needs TLS the LAN deployment lacks, plus new code entry and UI.
- **Keep reader-side verification with the reader downloading the key per
  tap:** it adds an extra round trip inside the RF time budget and
  exposes the key to the reader on every tap.

## Consequences
- Taps need the backend to authenticate phones. That is already true for
  every tap (the event is written there), so offline acceptance is not a
  regression.
- The pairing flow gains one phone-side step ("Link this phone").
- Build `hce.19`. Native suite 140/140. All four device envs build.
- Remaining limits (documented in `docs/HCE_PROTOCOL.md` §Security
  boundaries): unilateral authentication with a reader-chosen nonce
  (pre-play by a reader-key holder), a one-time NFC key exposure at
  enrollment, and the backend as symmetric trust root.
