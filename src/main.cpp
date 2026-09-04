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
 *   boot: Serial → reader.begin() → read mode-select pin → Wi-Fi (bounded)
 *         → indicate mode on the MODE LED
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
#include "secrets.h"

#include "CoreTypes.h"
#include "FeedbackController.h"
#include "Mode.h"
#include "Modes.h"
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

#if defined(PRESENCE_READER_IMPL_RC522)
static Rc522NfcReader reader(PIN_RC522_SS, PIN_RC522_RST);
#elif defined(PRESENCE_READER_IMPL_MOCK)
static MockSerialNfcReader reader(Serial);
#endif

static OperationMode operationMode;
static PairingMode pairingMode;
static Mode* mode = nullptr;  // selected at boot from the mode-select pin

// ---------------------------------------------------------------------------
// Boot / Arranque
// ---------------------------------------------------------------------------
static Mode* selectModeFromPin() {
    // Debounced boot-time read of the mode-select button (pin + polarity
    // documented in config.h and docs/HARDWARE_SETUP.md).
    int first = digitalRead(PIN_MODE_SELECT);
    delay(MODE_BUTTON_SETTLE_MS);
    int second = digitalRead(PIN_MODE_SELECT);

    const int level = (first == second) ? first
                                        : (digitalRead(PIN_MODE_SELECT) ? HIGH : LOW);
    return (level == MODE_LEVEL_PAIRING) ? static_cast<Mode*>(&pairingMode)
                                         : static_cast<Mode*>(&operationMode);
}

static void printBanner() {
    Serial.println();
    Serial.println("==============================================");
    Serial.println(" Presence Platform — NFC Reader / Lector NFC");
    Serial.println("==============================================");
    Serial.print("Reader impl / Implementacion: ");
    Serial.println(reader.label());
    Serial.print("Mode (button at boot) / Modo (boton al arrancar): ");
    Serial.println(mode->label());
    Serial.print("Backend: ");
    Serial.println(API_BASE_URL);
    Serial.println("---- type a UID + Enter to simulate a tap (mock build) ----");
    Serial.println("---- escribe un UID + Enter para simular un toque (build mock) ----");
}

void setup() {
    Serial.begin(115200);
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < BOOT_SERIAL_WAIT_MS) {
        // bounded wait for the Serial Monitor to attach
    }

    feedback.begin();
    feedback.indicate(FeedbackKind::BootConnecting);

    pinMode(PIN_MODE_SELECT, INPUT_PULLUP);
    mode = selectModeFromPin();

    if (!reader.begin()) {
        // RC522 init failed (wiring?) — log bilingual, keep booting so the
        // human sees network status and can re-check wiring.
        Serial.println("[!] NFC reader init failed / Fallo de init del lector NFC");
        Serial.println("    Check wiring: docs/HARDWARE_SETUP.md");
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
// Card tap pipeline (identical for both modes; the mode strategy decides
// the endpoint, the parser+interpreter decide the feedback)
// ---------------------------------------------------------------------------
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
                break;
            case TapOutcome::AuthFailure:
                Serial.println("[401] reader key rejected / clave de lector rechazada");
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
                break;
            case PairOutcome::AlreadyPaired:
                Serial.print("[422] ");
                Serial.println(result.message.c_str());
                break;
            case PairOutcome::AuthFailure:
                Serial.println("[401] reader key rejected / clave de lector rechazada");
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
