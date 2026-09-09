/**
 * station.h — the ESP32-CAM STATION: camera + RC522 + network + application
 * as ONE integrated device (env `esp32cam`, -DCAMERA_STATION).
 *
 * Lifecycle:
 *   begin()  → serial → status LED → shutter pin → camera (non-fatal) →
 *              RC522 (non-fatal, self-retrying) → Wi-Fi (bounded) →
 *              visualizer → banner. A failed peripheral NEVER halts the
 *              station; it retries in update() while the rest stays alive.
 *   update() → wifi.tick → led.tick → visualizer → serial console/capture
 *              trigger → shutter button → RC522 poll → presence tap
 *              pipeline → camera re-init retry. Non-blocking (bounded
 *              HTTP POSTs and capture delays excepted, as before).
 *
 * Application flow: an RFID tap resolves identity through the SAME
 * presence tap endpoint the reader uses, EXCEPT when a bottle-first
 * capture is pending (awaiting_card): then the tap UID associates that
 * capture directly (no new event). Otherwise, when the backend answers
 * next_step awaiting_classification with an event id, the station ARMS
 * that event — update() auto-captures+classifies once
 * CARD_FIRST_AUTO_CAPTURE_DELAY_MS elapses (card-first, auto;
 * BUTTON/ENTER captures immediately). A button/ENTER with nothing armed stays
 * bottle-first (capture → hold awaiting_card → tap/`a <uid>` associates).
 * Pairing mode, console password, debounce, Bearer auth and
 * multipart wire bytes are reused unchanged from the existing libs.
 */
#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include <string>

#include "config.h"

// Station credentials live ONLY in gitignored include/secrets.camera.h
// (own file — different backend identity from the reader's secrets.h).
// Fresh-checkout guard (TASK-001): without it we compile against the
// example placeholders and fail Wi-Fi/HTTP gracefully.
#if __has_include("secrets.camera.h")
#include "secrets.camera.h"
#else
#warning "include/secrets.camera.h not found — building with example placeholder values (cp include/secrets.camera.h.example include/secrets.camera.h)."
#include "secrets.camera.h.example"
#endif

#ifndef MODE_PASSWORD
#warning "MODE_PASSWORD not defined — using insecure default; add it to include/secrets.camera.h."
#define MODE_PASSWORD "CHANGE-ME-MODE-PW"
#endif

#include "CapturePayload.h"
#include "CaptureTrigger.h"
#include "CardDebouncer.h"
#include "CoreTypes.h"
#include "EspApiClient.h"
#include "ModeConsole.h"
#include "Modes.h"
#include "NfcReader.h"
#include "PayloadBuilder.h"
#include "Rc522NfcReader.h"
#include "ResponseParser.h"
#include "StationLed.h"
#include "WifiService.h"

namespace Presence {

class Station {
public:
    Station();
    void begin();
    void update();

private:
    // --- lifecycle helpers ------------------------------------------------
    void printBanner();
    void refreshStateLed();  // degraded vs idle, only on change
    bool initializeCamera();
    void showModeEvent(FeedbackKind kind);
    void switchMode();

    // --- serial: console password + capture trigger share one terminal ----
    void pollSerial();
    void dispatchSerialLine(const std::string& line, uint32_t now);

    // --- RFID → presence pipeline (moved from the reader, intact) ---------
    void handleCardTap(const std::string& uid);
    void printReaderKeyRemediation();

    // --- capture / upload (moved from the camera station, intact) ---------
    void freeLatestCapture();
    bool captureHighResolution();
    void flagCameraFailure();  // repeated runtime fb_get failures → re-init path
    void doCaptureAndUpload();
    void doAssociate(const std::string& uid);
    void handleCaptureCommand(const CaptureCommand& cmd);
    void expireStaleTransactions(uint32_t now);  // anti-steal timeouts
    void reportUpload(const char* what, int status, const String& body, bool transportOk);
    void chirpSuccess();  // no-op while PIN_CAM_BUZZER is -1 (no free pin on this bench)

    // --- visualizer ---------------------------------------------------------
    void setupRoutes();
    void sendJPEG(const uint8_t* data, size_t length);
    void handleStreamFrame();
    void handleCaptureImage();
    void handleCaptureStatus();
    void handleRoot();

    // --- members: ONE of each subsystem (this is the station) --------------
    WebServer server_;
    StationLed led_;
    WifiService wifi_;
    EspApiClient api_;
    Rc522NfcReader nfc_;
    CardDebouncer debouncer_;
    OperationMode operationMode_;
    PairingMode pairingMode_;
    Mode* mode_;
    ModeConsole console_;
    LineBuffer lines_;
    bool prevSerialWasCR_ = false;  // CRLF collapse: \n right after \r is one Enter
    TerminalCaptureTrigger trigger_;
    ButtonCaptureTrigger shutter_;

    // capture state (was file-static in camera/main.cpp)
    uint8_t* latestCapture_ = nullptr;
    size_t latestCaptureSize_ = 0;
    uint32_t latestCaptureId_ = 0;
    volatile bool cameraBusy_ = false;
    long backendCaptureId_ = -1;
    uint32_t backendCaptureAtMs_ = 0;  // when the pending capture was stored
    long armedEventId_ = -1;
    uint32_t armedAtMs_ = 0;  // when the card-first event was armed
    bool autoCaptureDone_ = false;  // auto photo fired once per arm (failures stay BUTTON-retryable)

    // recoverable subsystem state (STEP 8: never FATAL-halt)
    bool cameraOk_ = false;
    uint32_t lastCameraAttemptMs_ = 0;
    int cameraFailures_ = 0;  // consecutive fb_get failures → CAMERA_FAILURES_BEFORE_REINIT flips cameraOk_
    FeedbackKind lastIndicated_ = FeedbackKind::BootConnecting;
};

}  // namespace Presence
