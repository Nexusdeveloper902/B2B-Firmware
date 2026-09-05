/**
 * main.cpp — thin wiring of the firmware's swappable parts.
 * main.cpp — cablegado delgado de las partes intercambiables del firmware.
 *
 * Presence Platform — Reader Firmware (ESP32, PlatformIO, Arduino framework)
 * Task: TASK-001-reader-firmware-mvp
 *
 * The three-way interface split (NfcReader / Mode / FeedbackController)
 * means this file contains NO business logic — it wires interfaces:
 *
 *   boot: Serial → reader.begin() → Wi-Fi (bounded) → indicate mode on
 *         the MODE LED (boots OPERATION; ADR-005)
 *   loop: wifi.tick → reader.poll → debouncer → mode.onCardTap →
 *         api.post → ResponseParser → mode.interpret → feedback.showEvent
 *         (all non-blocking; no delay() in loop)
 *
 * Bilingual: serial banner + status lines are printed in English and
 * Spanish (the platform's established convention).
 * / Bilingüe: banner y líneas de estado en inglés y español.
 */

#include <Arduino.h>

#include <string>

#include "config.h"

// Real credentials live ONLY in the gitignored include/secrets.h.
// A fresh checkout (no secrets.h yet) still compiles — against the
// placeholder values of secrets.h.example — so `pio run` works before
// the developer copies the template. The device will then fail Wi-Fi/
// HTTP gracefully (bounded, NetworkError feedback) until real values
// are provided. / Las credenciales reales viven SOLO en el gitignored
// include/secrets.h. Un checkout nuevo aún compila — con los valores de
// marcador de secrets.h.example — hasta que se copien valores reales.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "include/secrets.h not found — building with example placeholder values (cp include/secrets.h.example include/secrets.h). / No se encontró include/secrets.h — se compila con valores de ejemplo."
#include "secrets.h.example"
#endif

// TASK-003: a secrets.h written before the mode-console era has no
// MODE_PASSWORD — keep it compiling (insecure default + bilingual
// #warning) instead of breaking the user's local file after a pull.
// / TASK-003: un secrets.h anterior a la consola de modo no tiene
// MODE_PASSWORD — sigue compilando (valor inseguro + #warning bilingüe).
#ifndef MODE_PASSWORD
#warning "MODE_PASSWORD not defined — using insecure default; add it to include/secrets.h. / MODE_PASSWORD no definido — valor por defecto inseguro; añádelo a include/secrets.h."
#define MODE_PASSWORD "CHANGE-ME-MODE-PW"
#endif

#include "CoreTypes.h"
#include "FeedbackController.h"
#include "Mode.h"
#include "Modes.h"
#include "ModeConsole.h"
#include "NfcReader.h"
#include "ResponseParser.h"
#include "CardDebouncer.h"

#if defined(PRESENCE_READER_IMPL_RC522)
#include "Rc522NfcReader.h"
#elif defined(PRESENCE_READER_IMPL_MOCK)
#include "MockSerialNfcReader.h"
#else
#error "Select a reader implementation: build env esp32dev (RC522) or esp32dev-mock"
#endif

#include "LedFeedbackController.h"
#include "EspApiClient.h"
#include "WifiService.h"

using namespace Presence;

// ---------------------------------------------------------------------------
// Composition root (the ONLY place concrete classes meet)
// ---------------------------------------------------------------------------
static WifiService wifi(WIFI_SSID, WIFI_PASSWORD,
                        WIFI_CONNECT_TIMEOUT_MS, WIFI_RECONNECT_INTERVAL_MS);
static EspApiClient api(API_BASE_URL, READER_API_KEY, HTTP_TIMEOUT_MS);
static LedFeedbackController feedback(PIN_LED_MODE, PIN_LED_EVENT, PIN_BUZZER);
static CardDebouncer debouncer(CARD_COOLDOWN_MS);

// TASK-003: the serial console that gates mode switching. Password value
// from secrets.h; knobs (max wrong attempts, lockout) from config.h.
static ModeConsole modeConsole(MODE_PASSWORD,
                                MODE_CONSOLE_MAX_WRONG_ATTEMPTS,
                                MODE_CONSOLE_LOCKOUT_MS);
static LineBuffer serialInput(SERIAL_LINE_MAX_LENGTH);

#if defined(PRESENCE_READER_IMPL_RC522)
static Rc522NfcReader reader(PIN_RC522_SS, PIN_RC522_RST,
                             PIN_RC522_SCK, PIN_RC522_MISO, PIN_RC522_MOSI,
                             &Serial);  // Serial = diagnostics sink
#elif defined(PRESENCE_READER_IMPL_MOCK)
static MockSerialNfcReader reader;
#endif

static OperationMode operationMode;
static PairingMode pairingMode;
static Mode* mode = nullptr;  // boots OPERATION; toggled by the console password

// ---------------------------------------------------------------------------
// Boot / Arranque
// ---------------------------------------------------------------------------
static void printBanner() {
    Serial.println();
    Serial.println("==============================================");
    Serial.println(" Presence Platform — NFC Reader / Lector NFC");
    Serial.println("==============================================");
    Serial.print("Reader impl / Implementacion: ");
    Serial.println(reader.label());
    Serial.print("Mode / Modo: ");
    Serial.println(mode->label());
    Serial.print("Backend: ");
    Serial.println(API_BASE_URL);
    Serial.println("---- type the MODE PASSWORD + Enter to switch modes / escribe la");
    Serial.println("     CLAVE DE MODO + Enter para cambiar de modo (secrets.h) ----");
#if defined(PRESENCE_READER_IMPL_RC522)
    Serial.println("---- present a card to the reader / presenta una tarjeta al lector ----");
#elif defined(PRESENCE_READER_IMPL_MOCK)
    Serial.println("---- type a UID + Enter to simulate a tap (mock build) ----");
    Serial.println("---- escribe un UID + Enter para simular un toque (build mock) ----");
#endif
}

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < BOOT_SERIAL_WAIT_MS) {
        // bounded wait for the Serial Monitor to attach
    }

    feedback.begin();
    feedback.indicate(FeedbackKind::BootConnecting);

    // TASK-003: mode no longer comes from a boot pin — the device boots in
    // OPERATION and the operator toggles it anytime with the console
    // password (ADR-005; the mode button is gone).
    mode = &operationMode;

    if (!reader.begin()) {
        // RC522 init failed — the driver already printed the probed
        // version + expected pins + 3.3 V reminder. Keep booting: the
        // device retries init every RC522_REINIT_INTERVAL_MS in loop(),
        // and the human still sees network status meanwhile.
        // / Fallo de init — el driver ya imprimió la versión sondeada, los
        // pines esperados y el recordatorio de 3.3 V. El arranque sigue:
        // loop() reintenta cada RC522_REINIT_INTERVAL_MS.
        Serial.println("[!] NFC reader init failed — will keep retrying /");
        Serial.println("    fallo de init del lector — se seguira reintentando");
        Serial.println("    Wiring table: docs/HARDWARE_SETUP.md");
    }

    printBanner();

    const bool connected = wifi.begin(millis());
    Serial.print("[WiFi] ");
    if (connected) {
        Serial.print("connected / conectado — IP: ");
        Serial.println(wifi.ip().c_str());
    } else {
        Serial.println("NOT connected yet — retrying in background /");
        Serial.println("aun no conectado — reintentando en segundo plano");
    }

    // Continuous mode indication from here on (not just at boot).
    feedback.indicate(mode->kind() == ModeKind::Pairing ? FeedbackKind::IdlePairing
                                                        : FeedbackKind::IdleOperation);
    debouncer.reset();
}

// ---------------------------------------------------------------------------
// Mode switching (TASK-003) — serial console password / Contraseña por
// consola serial
// ---------------------------------------------------------------------------
static void showModeEvent(FeedbackKind kind) {
    FeedbackSignal signal;  // C++11: no brace-init with member defaults
    signal.kind = kind;
    feedback.showEvent(signal);
}

static void switchMode() {
    mode = (mode->kind() == ModeKind::Pairing) ? static_cast<Mode*>(&operationMode)
                                               : static_cast<Mode*>(&pairingMode);
    Serial.println();
    Serial.print("[MODE] switched to / cambiado a: ");
    Serial.println(mode->label());

    // TASK-004: the device teaches its own flow — the strategy's hint says
    // what to do NEXT in this mode (pairing: arm a session first, then a
    // fresh card; operation: tap paired cards).
    // / El dispositivo enseña su flujo — la pista de la estrategia dice
    // qué hacer AHORA en este modo.
    Serial.print("[MODE] ");
    Serial.print(mode->hint());  // bilingual; may be multi-line
    Serial.println();

    // EVENT LED: operator acknowledgment; MODE LED: the new idle pattern
    // (the continuous mode indication is the source of truth on the bench).
    showModeEvent(FeedbackKind::ModeSwitched);
    feedback.indicate(mode->kind() == ModeKind::Pairing ? FeedbackKind::IdlePairing
                                                        : FeedbackKind::IdleOperation);
    debouncer.reset();  // a tap in flight must not straddle the switch
}

static void dispatchConsoleLine(const std::string& line, uint32_t now) {
    if (line.empty()) {
        return;  // bare Enter / discarded overflow line — nothing to do
    }

#if defined(PRESENCE_READER_IMPL_MOCK)
    // Mock build: Serial carries BOTH console lines and virtual taps.
    // The password gets the console treatment; anything else is a card
    // UID (the documented dev workflow). Wrong "passwords" here are
    // almost always UIDs, so the lockout is not armed in this build.
    // / Build simulado: lo que no es la contraseña es un UID virtual.
    if (modeConsole.matches(line)) {
        modeConsole.handleLine(line, now);  // -> Accepted (matches)
        switchMode();
    } else {
        reader.pushLine(line);  // virtual card tap
    }
#else
    // Real-reader build: the console owns the whole terminal — every
    // non-empty line is a password attempt (with lockout after the
    // configured wrongs).
    // / Build con lector real: toda línea no vacía es un intento de clave.
    const ConsoleResult result = modeConsole.handleLine(line, now);
    switch (result) {
        case ConsoleResult::Accepted:
            switchMode();
            break;
        case ConsoleResult::Rejected:
            Serial.println();
            Serial.print("[MODE] wrong password / clave incorrecta — ");
            if (modeConsole.lockedOut(now)) {
                Serial.print("input locked for ");
                Serial.print(MODE_CONSOLE_LOCKOUT_MS / 1000);
                Serial.println(" s / entrada bloqueada");
            } else {
                Serial.print(MODE_CONSOLE_MAX_WRONG_ATTEMPTS - modeConsole.wrongAttempts());
                Serial.println(" attempt(s) left / intento(s) restante(s)");
            }
            showModeEvent(FeedbackKind::ModeRejected);
            break;
        case ConsoleResult::LockedOut:
            Serial.println();
            Serial.println("[MODE] input locked — wait for the countdown /");
            Serial.println("     entrada bloqueada — espera la cuenta atras");
            showModeEvent(FeedbackKind::ModeRejected);
            break;
        case ConsoleResult::Ignored:
        default:
            break;
    }
#endif
}

/** Non-blocking Serial reader: chars → masked echo → lines → dispatch. */
static void pollConsole(uint32_t now) {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c != '\n' && c != '\r') {
            Serial.print('*');  // masked echo: never print what was typed
        }
        std::string line;
        if (serialInput.feed(c, line)) {
            Serial.println();  // end the asterisk row on Enter
            if (serialInput.overflowed()) {
                Serial.println("[MODE] input too long — line discarded /");
                Serial.println("     entrada demasiado larga — linea descartada");
            }
            dispatchConsoleLine(line, now);
        }
    }
}

// ---------------------------------------------------------------------------
// Card tap pipeline (identical for both modes; the mode strategy decides
// the endpoint, the parser+interpreter decide the feedback)
// ---------------------------------------------------------------------------

// TASK-004: a 401 is always a key-PROVISIONING failure (the backend has
// no readers row matching this READER_API_KEY) — point the operator at
// the fix instead of a bare rejection. Printed for both modes.
// / Un 401 siempre es un fallo de PROVISIONAMIENTO de la clave — señala
// la solución en vez de un rechazo pelado. Se imprime en ambos modos.
static void printReaderKeyRemediation() {
    Serial.println("     READER_API_KEY has no matching reader row on the backend /");
    Serial.println("     READER_API_KEY no tiene una fila de lector en el backend —");
    Serial.println("     fix / arreglo: docs/PAIRING.md (provisioning / provisionamiento)");
}

static void handleCardTap(const std::string& uid) {
    Serial.println();
    Serial.print("[NFC] card / tarjeta: ");
    Serial.println(uid.c_str());

    // 1) The mode strategy decides WHICH call this tap becomes.
    ApiCall call = mode->onCardTap(uid);

    // 2) Transport (never throws; failures → status < 0).
    HttpResponse response = api.post(call.path, call.jsonBody);

    // 3) Locale-independent parsing + mode-specific interpretation.
    if (call.type == ApiCallType::Tap) {
        TapResult result = parseTapResponse(response.status, response.body);
        FeedbackSignal signal = operationMode.interpret(result);

        switch (result.outcome) {
            case TapOutcome::Success:
                Serial.print("[OK] event logged / evento registrado — ");
                Serial.print(result.studentFirstName.c_str());
                Serial.print(" (");
                Serial.print(result.eventType.c_str());
                Serial.println(")");
                if (result.awaitingClassification) {
                    Serial.println("[i] next_step: awaiting_classification");
                }
                break;
            case TapOutcome::CardNotRecognized:
                Serial.print("[404] ");
                Serial.println(result.message.c_str());
                // TASK-004: the natural next step for an unknown card is
                // pairing — say so. / El siguiente paso natural de una
                // tarjeta desconocida es emparejarla — decirlo.
                Serial.println("     unpaired card? switch to PAIRING and arm a session —");
                Serial.println("     docs/PAIRING.md / ¿tarjeta sin emparejar? cambia a");
                Serial.println("     EMPAREJAR y arma una sesion — docs/PAIRING.md");
                break;
            case TapOutcome::AuthFailure:
                Serial.println("[401] reader key rejected / clave de lector rechazada");
                printReaderKeyRemediation();
                break;
            case TapOutcome::NetworkError:
                Serial.println("[NET] network failure / fallo de red");
                break;
            default:
                Serial.print("[ERR] unexpected / inesperado: ");
                Serial.println(result.message.c_str());
                break;
        }
        feedback.showEvent(signal);
    } else {  // ApiCallType::PairCard
        PairResult result = parsePairResponse(response.status, response.body);
        FeedbackSignal signal = pairingMode.interpret(result);

        switch (result.outcome) {
            case PairOutcome::Success:
                Serial.print("[OK] card paired to / tarjeta emparejada con: ");
                Serial.println(result.pairedStudentName.c_str());
                break;
            case PairOutcome::NoActiveSession:
                Serial.print("[409] ");
                Serial.println(result.message.c_str());
                // TASK-004: teach the arm-then-pair flow at the moment it
                // bites. / Enseñar el flujo arma-then-pair justo cuando muerde.
                Serial.println("     arm a session first, then tap within the window —");
                Serial.println("     docs/PAIRING.md / arma primero una sesion y acerca");
                Serial.println("     la tarjeta dentro de la ventana — docs/PAIRING.md");
                break;
            case PairOutcome::AlreadyPaired:
                Serial.print("[422] ");
                Serial.println(result.message.c_str());
                // The backend does NOT consume the session on a 422 — the
                // operator can retry immediately with a fresh card.
                Serial.println("     use a FRESH card — the session stays armed / usa una");
                Serial.println("     tarjeta NUEVA — la sesion sigue armada");
                break;
            case PairOutcome::AuthFailure:
                Serial.println("[401] reader key rejected / clave de lector rechazada");
                printReaderKeyRemediation();
                break;
            case PairOutcome::NetworkError:
                Serial.println("[NET] network failure / fallo de red");
                break;
            default:
                Serial.print("[ERR] unexpected / inesperado: ");
                Serial.println(result.message.c_str());
                break;
        }
        feedback.showEvent(signal);
    }
}

// ---------------------------------------------------------------------------
// Main loop — non-blocking / Bucle principal — no bloqueante
// ---------------------------------------------------------------------------
void loop() {
    const uint32_t now = millis();

    wifi.tick(now);
    feedback.tick(now);
    pollConsole(now);  // TASK-003: serial input → console dispatch

    std::string uid;
    if (reader.poll(uid)) {
        if (debouncer.shouldProcess(uid, now)) {
            handleCardTap(uid);
        }
    } else {
        debouncer.markAbsent();  // no card present → resting detection
    }

    // No delay(): pattern stepping, Wi-Fi maintenance and polling continue.
}
