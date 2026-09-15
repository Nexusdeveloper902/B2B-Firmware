/**
 * HceProtocol.h — the Pulse Android-HCE credential protocol (reader side).
 * HceProtocol.h — protocolo de credencial HCE de Android de Pulse (lado lector).
 *
 * Byte-exact mirror of the Android ApduProtocol object and the canonical
 * spec in docs/HCE_PROTOCOL.md: SELECT AID F0010203040506, then an
 * 8-byte CHALLENGE answered with len + credId + HMAC-SHA256(HCE_SECRET,
 * credId || nonce) + 9000.
 *
 * Pure C++ (no Arduino headers) so the whole application protocol —
 * builders, parsers AND the HMAC verification — is host-testable in the
 * `native` env. The RF/ISO-DEP transport (RATS, I-blocks) stays in
 * Rc522NfcReader, which owns the MFRC522Extended driver.
 * / C++ puro (sin cabeceras de Arduino): todo el protocolo de aplicación
 * es testeable en el host. El transporte RF/ISO-DEP vive en Rc522NfcReader.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace Presence {
namespace Hce {

// --- Application layer ------------------------------------------------------
// Proprietary 'F' category AID: valid, consistent on both sides, collides
// with nothing production. Changing it breaks every installed phone app.
static const uint8_t AID[] = {0xF0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
static const size_t AID_LEN = sizeof(AID);

static const uint8_t CLA_PROTO = 0x00;  // SELECT AID
static const uint8_t INS_SELECT = 0xA4;
static const uint8_t CLA_AUTH = 0x80;  // prototype auth commands
static const uint8_t INS_CHALLENGE = 0x10;

static const size_t NONCE_LEN = 8;    // reader challenge length
static const size_t HMAC_LEN = 32;    // HMAC-SHA256 length
static const size_t MAX_CRED_LEN = 32;  // longest credential id accepted

// RC522 FIFO = 64 B: every message below fits one I-block (~57 B INF),
// so no phone-side chaining is ever needed.

/** Builds: 00 A4 04 00 Lc AID (no Le). Returns bytes written (12), 0 when cap is short. */
size_t buildSelectAid(uint8_t* out, size_t cap);

/** Builds: 80 10 00 00 Lc nonce (no Le). Returns bytes written (13), 0 when cap is short. */
size_t buildChallenge(const uint8_t nonce[NONCE_LEN], uint8_t* out, size_t cap);

/** True when the response ends in 90 00 (at least the status word present). */
bool isOk(const uint8_t* resp, size_t len);

enum class SelectOutcome {
    Selected,    // "HCE-OK" + 9000: a Pulse HCE phone answered
    UnknownAid,  // 6A82: something else answered (wrong app / AID mismatch)
    Malformed,   // anything else: truncated, wrong payload, wrong SW
};

/** Classifies one SELECT AID response. Never throws. */
SelectOutcome parseSelectResponse(const uint8_t* resp, size_t len);

struct ChallengeResponse {
    bool ok = false;
    std::string credId;  // application-level identity (NOT the RF UID)
    uint8_t mac[HMAC_LEN];
};

/**
 * Parses one CHALLENGE response: len(1) + credId + HMAC(32) + 9000.
 * Returns true (and fills out) only on an exact-length, 9000-terminated
 * frame. The RF UID plays no role here by construction — these parsers
 * do not even take one. / El UID RF no interviene por construcción.
 */
bool parseChallengeResponse(const uint8_t* resp, size_t len, ChallengeResponse& out);

/** HMAC-SHA256 (compact implementation, no dependencies). */
void hmacSha256(const uint8_t* key, size_t keyLen,
                const uint8_t* msg, size_t msgLen,
                uint8_t out[HMAC_LEN]);

/**
 * Recomputes HMAC-SHA256(key, credId || nonce) and compares in constant
 * time. Secrets are never logged — callers must only log the boolean.
 */
bool verifyChallengeResponse(const ChallengeResponse& r,
                             const uint8_t nonce[NONCE_LEN],
                             const uint8_t* key, size_t keyLen);

}  // namespace Hce
}  // namespace Presence
