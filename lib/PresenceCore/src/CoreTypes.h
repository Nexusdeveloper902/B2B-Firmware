/**
 * CoreTypes.h — shared value types of the reader firmware.
 * CoreTypes.h — tipos compartidos del firmware del lector.
 *
 * PresenceCore is deliberately FREE of Arduino headers so every rule in
 * this library can be unit-tested on the host (PlatformIO native env)
 * and reused by both the RC522 and the mock reader build.
 * PresenceCore no incluye cabeceras de Arduino para que toda esta lógica
 * sea testeable en el host y reutilizable por el lector RC522 y el simulado.
 */
#pragma once

#include <cstdint>
#include <string>

namespace Presence {

// ---------------------------------------------------------------------------
// Operating modes / Modos de operación
// ---------------------------------------------------------------------------
enum class ModeKind {
    Operation,  // normal use: tap a paired card → presence event / uso normal
    Pairing,    // associate a fresh card's UID with a student / emparejar
};

const char* modeKindToString(ModeKind kind);

// ---------------------------------------------------------------------------
// API calls the firmware can make / Llamadas que el firmware puede hacer
// ---------------------------------------------------------------------------
enum class ApiCallType {
    Tap,       // POST /api/v1/events/tap          (operation mode)
    PairCard,  // POST /api/v1/admin/cards/pair     (pairing mode)
};

struct ApiCall {
    ApiCallType type = ApiCallType::Tap;
    std::string path;      // URL path, e.g. "/api/v1/events/tap"
    std::string jsonBody;  // serialized JSON request payload
};

// TASK-007: the full Authorization header VALUE for every request this
// firmware makes: "Bearer " + the static reader key. That exact scheme is
// the B2B-Core contract (ADR-002 there) — Laravel's bearerToken() only
// reads "Authorization: Bearer <key>".
// / TASK-007: el VALOR completo de la cabecera Authorization de toda
// peticion de este firmware: "Bearer " + la clave estatica del lector.
// Ese esquema exacto es el contrato de B2B-Core (su ADR-002).
//
// WHY it lives here as a helper: the ESP32 Arduino HTTPClient's
// setAuthorization(key) prefixes the value with its DEFAULT
// authorization type — "Basic" — producing "Authorization: Basic <key>".
// Laravel ignores that header, so every real-hardware call answered 401
// with a perfectly valid, correctly provisioned key (curl verification
// never caught it: curl sends the header verbatim). The literal scheme
// lives HERE, in host-testable code, so test_auth.cpp can pin it and a
// regression cannot hide inside a transport-only header again.
// / ¿Por que un helper aqui? setAuthorization(key) de HTTPClient prefija
// "Basic" (su tipo por defecto) y Laravel lo ignora: todo el hardware
// real recibia 401 con una clave valida. El esquema literal vive AQUI,
// en codigo testeable en el host, para que test_auth.cpp lo fije.
inline std::string bearerAuthorizationValue(const std::string& bearerKey) {
    return "Bearer " + bearerKey;
}

// ---------------------------------------------------------------------------
// Tap outcomes (operation mode) / Resultados de tap (modo operación)
// ---------------------------------------------------------------------------
enum class TapOutcome {
    Success,           // 200 {"status":"ok"}
    CardNotRecognized, // 404 (unknown card, or card not active)
    AuthFailure,       // 401 missing/invalid reader key
    ValidationError,   // 422 (malformed payload — should not happen in practice)
    ServerError,       // any other 5xx
    NetworkError,      // transport-level failure (timeout, DNS, connection)
    UnknownError       // anything unrecognized — device must stay responsive
};

struct TapResult {
    TapOutcome outcome = TapOutcome::UnknownError;
    std::string studentFirstName;    // on success — device feedback
    std::string eventType;           // e.g. CLASS_ATTENDANCE
    int32_t eventId = -1;            // backend event id
    bool awaitingClassification = false;  // recycling readers: next_step
    std::string message;             // raw server message, for the serial log
};

// ---------------------------------------------------------------------------
// Pairing outcomes (pairing mode) / Resultados de emparejamiento (modo emparejar)
// ---------------------------------------------------------------------------
enum class PairOutcome {
    Success,          // 200 {"status":"ok"} — card linked to student
    NoActiveSession,  // 409 — no armed pairing session
    AlreadyPaired,    // 422 — credential_uid already linked to a card
    AuthFailure,      // 401 missing/invalid reader key
    ValidationError,  // other 422 (malformed body)
    ServerError,      // any other 5xx
    NetworkError,     // transport-level failure
    UnknownError      // anything unrecognized — device must stay responsive
};

struct PairResult {
    PairOutcome outcome = PairOutcome::UnknownError;
    std::string pairedStudentName;  // on success — show/log the student
    int32_t studentId = -1;
    std::string message;            // raw server message, for the serial log
};

// ---------------------------------------------------------------------------
// Feedback signals / Señales de retroalimentación
// ---------------------------------------------------------------------------
// The single vocabulary the FeedbackController understands. Mode logic and
// networking NEVER touch LEDs directly — they emit FeedbackSignals.
// Vocabulario único que entiende el FeedbackController. La lógica de modos y
// la red NUNCA tocan LEDs directamente — emiten FeedbackSignals.
enum class FeedbackKind {
    // continuous states / estados continuos
    BootConnecting,   // boot + Wi-Fi association in progress
    IdleOperation,    // idle in operation mode
    IdlePairing,      // idle in pairing mode
    StationDegraded,  // station alive but a subsystem is down (camera/NFC/net)
    // operation-mode events / eventos del modo operación
    TapSuccess,       // presence event logged
    TapRejected,      // 404 — card not recognized / not active
    CaptureSuccess,   // station: image captured + uploaded (HTTP 200)
    // pairing-mode events / eventos del modo emparejar
    PairSuccess,      // card paired to the student
    PairNoSession,    // 409 — no pairing session active
    PairAlreadyPaired,// 422 — card already linked
    // operator console events / eventos de la consola del operador
    ModeSwitched,     // correct mode password — mode toggled (TASK-003)
    ModeRejected,     // wrong mode password (TASK-003)
    // cross-mode failures / fallos comunes
    AuthError,        // 401 — reader key rejected
    NetworkError,     // transport failure
    ServerError       // unexpected backend response
};

struct FeedbackSignal {
    FeedbackKind kind = FeedbackKind::TapSuccess;
    std::string detail;  // human-facing extra (student name, message) → log
};

}  // namespace Presence
