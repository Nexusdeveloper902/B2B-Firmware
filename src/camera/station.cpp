/**
 * station.cpp — Station lifecycle + subsystem integration.
 * See station.h for the architecture. Code below is MOVED (not rewritten)
 * from src/main.cpp (tap pipeline, console) and the former camera-only
 * src/camera/main.cpp (capture, upload, visualizer), with two behavior
 * changes: (1) camera/Wi-Fi failures are recoverable states, never halts;
 * (2) a tap whose backend answer is next_step awaiting_classification
 * auto-captures+classifies in the same transaction (no new protocol —
 * the existing classifyWithEvent path).
 */
#include "station.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include "esp_camera.h"
#include "esp_heap_caps.h"

namespace Presence {

namespace {

// Resolutions (reference values, preserved).
constexpr framesize_t kStreamResolution = FRAMESIZE_VGA;
constexpr framesize_t kCaptureResolution = FRAMESIZE_XGA;
constexpr framesize_t kInitialFrameSize = FRAMESIZE_UXGA;

long extractLongField(const String& body, const char* field) {
    String needle = String("\"") + field + "\":";
    int at = body.indexOf(needle);
    if (at < 0) {
        return -1;
    }
    at += needle.length();
    String digits;
    while (at < (int) body.length() && (isdigit(body[(unsigned) at]) || body[(unsigned) at] == '-')) {
        digits += body[(unsigned) at++];
    }
    return digits.length() ? digits.toInt() : -1;
}

}  // namespace

Station::Station()
    : server_(80),
      led_(PIN_STATION_LED, /*activeLow=*/true),
      wifi_(WIFI_SSID, WIFI_PASSWORD, WIFI_CONNECT_TIMEOUT_MS, WIFI_RECONNECT_INTERVAL_MS),
      api_(API_BASE_URL, READER_API_KEY, HTTP_TIMEOUT_MS),
      nfc_(PIN_RC522_SS, PIN_RC522_RST, PIN_RC522_SCK, PIN_RC522_MISO, PIN_RC522_MOSI, &Serial),
      debouncer_(CARD_COOLDOWN_MS),
      mode_(&operationMode_),
      console_(MODE_PASSWORD, MODE_CONSOLE_MAX_WRONG_ATTEMPTS, MODE_CONSOLE_LOCKOUT_MS),
      lines_(SERIAL_LINE_MAX_LENGTH),
      trigger_([]() { return millis(); }),
      shutter_([]() { return digitalRead(PIN_SHUTTER_BUTTON) == LOW; },
               []() { return millis(); }, SHUTTER_DEBOUNCE_MS) {}

// ---------------------------------------------------------------------------
// begin()
// ---------------------------------------------------------------------------

void Station::begin() {
    Serial.begin(115200);
    delay(1000);

    led_.begin();
    led_.indicate(FeedbackKind::BootConnecting);

    pinMode(PIN_SHUTTER_BUTTON, INPUT_PULLUP);  // shutter: button to GND
    if (PIN_CAM_BUZZER >= 0) {
        pinMode(PIN_CAM_BUZZER, OUTPUT);
        digitalWrite(PIN_CAM_BUZZER, LOW);
    }

    Serial.println();
    Serial.println("================================");
    Serial.println("PRESENCE PLATFORM — ESP32-CAM STATION");
    Serial.println("ESTACION ESP32-CAM — PRESENCE PLATFORM");
    Serial.println("(camera + RC522 + presence, one device)");
    Serial.println("================================");

    if (strstr(WIFI_SSID, "YOUR_") != nullptr ||
        strstr(WIFI_PASSWORD, "YOUR_") != nullptr ||
        strstr(READER_API_KEY, "00000000000000000000000000000000") != nullptr) {
        Serial.println("[EN] WARNING: secrets.camera.h still contains placeholder values —");
        Serial.println("     edit include/secrets.camera.h (WIFI_SSID, WIFI_PASSWORD, API_BASE_URL, READER_API_KEY).");
        Serial.println("[ES] AVISO: secrets.camera.h aún tiene valores de marcador —");
        Serial.println("     edita include/secrets.camera.h (WIFI_SSID, WIFI_PASSWORD, API_BASE_URL, READER_API_KEY).");
    }

    // Camera: recoverable, never FATAL — the station retries in update().
    cameraOk_ = initializeCamera();
    lastCameraAttemptMs_ = millis();
    if (!cameraOk_) {
        Serial.println("[!] Camera init failed — station continues, retrying /");
        Serial.println("    fallo de init de cámara — la estación sigue, reintentando");
    }

    // RC522: non-fatal, self-retrying (see Rc522NfcReader).
    if (!nfc_.begin()) {
        Serial.println("[!] NFC reader init failed — will keep retrying /");
        Serial.println("    fallo de init del lector — se seguira reintentando");
        Serial.println("    Wiring table: docs/CAMERA_STATION.md");
    }

    const bool connected = wifi_.begin(millis());
    WiFi.setSleep(false);  // streaming station: modem-sleep off (was in connectWiFi)
    Serial.print("[WiFi] ");
    if (connected) {
        Serial.print("connected / conectado — IP: ");
        Serial.println(wifi_.ip().c_str());
    } else {
        Serial.println("NOT connected yet — retrying in background /");
        Serial.println("aun no conectado — reintentando en segundo plano");
    }

    setupRoutes();
    server_.begin();

    printBanner();
    refreshStateLed();
    debouncer_.reset();
}

void Station::printBanner() {
    Serial.println();
    Serial.println("================================");
    Serial.println("SERVER READY / SERVIDOR LISTO");
    Serial.println("================================");
    Serial.print("[EN] Visualizer: http://");
    Serial.print(wifi_.ip().c_str());
    Serial.println("/");
    Serial.print("Reader impl / Implementacion: ");
    Serial.println(nfc_.label());
    Serial.print("Mode / Modo: ");
    Serial.println(mode_->label());
    Serial.print("Backend: ");
    Serial.println(API_BASE_URL);
    Serial.println();
    Serial.println("[EN] Serial commands / [ES] Comandos seriales:");
    Serial.println("  ENTER            capture + upload (bottle-first) / capturar + subir (botella-primero)");
    Serial.println("  a <credential_uid>  associate last capture with this card / asociar la última captura con esta tarjeta");
    Serial.println("  e <event_id>     arm card-first classify for next ENTER / armar clasificación tarjeta-primero para el próximo ENTER");
    Serial.println("  c                local capture only (no upload) / captura local sin subir");
    Serial.println("  BUTTON (GPIO12->GND) same as ENTER / igual que ENTER");
    Serial.println("  MODE PASSWORD + Enter switches operation/pairing (masked, lockout-guarded)");
    Serial.println();
}

// ---------------------------------------------------------------------------
// update()
// ---------------------------------------------------------------------------

void Station::update() {
    const uint32_t now = millis();

    wifi_.tick(now);
    led_.tick(now);
    server_.handleClient();
    pollSerial();

    if (shutter_.poll()) {
        handleCaptureCommand({CaptureCommand::Capture, ""});  // button == ENTER
    }

    std::string uid;
    if (nfc_.poll(uid)) {
        if (debouncer_.shouldProcess(uid, now)) {
            handleCardTap(uid);
        }
    } else {
        debouncer_.markAbsent();  // no card present → resting detection
    }

    if (!cameraOk_ && (now - lastCameraAttemptMs_) >= CAMERA_REINIT_INTERVAL_MS) {
        lastCameraAttemptMs_ = now;
        if (initializeCamera()) {
            cameraOk_ = true;
            Serial.println("[CAM] recovered / cámara recuperada");
        }
    }

    refreshStateLed();
}

void Station::refreshStateLed() {
    const bool degraded = !cameraOk_ || !nfc_.healthy() || !wifi_.isConnected();
    const FeedbackKind want = degraded ? FeedbackKind::StationDegraded
                              : (mode_->kind() == ModeKind::Pairing ? FeedbackKind::IdlePairing
                                                                   : FeedbackKind::IdleOperation);
    if (want != lastIndicated_) {
        lastIndicated_ = want;
        led_.indicate(want);
    }
}

// ---------------------------------------------------------------------------
// Camera (reference init + capture, preserved)
// ---------------------------------------------------------------------------

bool Station::initializeCamera() {
    camera_config_t config = {};

    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    if (psramFound()) {
        Serial.println("PSRAM detected. / PSRAM detectada.");
        config.frame_size = kInitialFrameSize;
        config.jpeg_quality = 10;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    } else {
        Serial.println("WARNING: No PSRAM detected. / AVISO: sin PSRAM.");
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera initialization failed: 0x%x / Inicialización de cámara falló: 0x%x\n", err);
        return false;
    }

    sensor_t* sensor = esp_camera_sensor_get();
    if (sensor == nullptr) {
        Serial.println("ERROR: Sensor unavailable. / ERROR: sensor no disponible.");
        return false;
    }

    Serial.printf("Camera PID: 0x%02X\n", sensor->id.PID);

    if (sensor->id.PID == OV3660_PID) {
        sensor->set_vflip(sensor, 1);
        sensor->set_brightness(sensor, 1);
        sensor->set_saturation(sensor, -2);
    }

    sensor->set_framesize(sensor, kStreamResolution);
    Serial.println("Camera started at VGA. / Cámara iniciada en VGA.");
    return true;
}

void Station::freeLatestCapture() {
    if (latestCapture_ != nullptr) {
        heap_caps_free(latestCapture_);
        latestCapture_ = nullptr;
    }
    latestCaptureSize_ = 0;
}

bool Station::captureHighResolution() {
    if (!cameraOk_) {
        Serial.println("[CAM] camera down — capture skipped, retry pending /");
        Serial.println("      cámara caída — captura omitida, reintento pendiente");
        return false;
    }
    if (cameraBusy_) {
        Serial.println("Camera is already busy. / La cámara está ocupada.");
        return false;
    }

    cameraBusy_ = true;

    Serial.println();
    Serial.println("================================");
    Serial.println("HIGH-RES CAPTURE / CAPTURA ALTA RESOLUCIÓN");
    Serial.println("================================");

    sensor_t* sensor = esp_camera_sensor_get();

    if (sensor == nullptr) {
        Serial.println("ERROR: camera sensor unavailable. / ERROR: sensor no disponible.");
        cameraBusy_ = false;
        return false;
    }

    Serial.println("Switching sensor to XGA... / Cambiando sensor a XGA...");

    int result = sensor->set_framesize(sensor, kCaptureResolution);
    if (result != 0) {
        Serial.printf("ERROR: set_framesize failed: %d\n", result);
        sensor->set_framesize(sensor, kStreamResolution);
        cameraBusy_ = false;
        return false;
    }

    delay(250);

    camera_fb_t* discard = esp_camera_fb_get();
    if (discard != nullptr) {
        esp_camera_fb_return(discard);
    } else {
        Serial.println("WARNING: first post-switch frame was unavailable. / AVISO: el primer frame tras el cambio no estaba disponible.");
    }

    delay(50);

    camera_fb_t* fb = esp_camera_fb_get();
    if (fb == nullptr) {
        Serial.println("ERROR: Camera capture failed. / ERROR: la captura falló.");
        sensor->set_framesize(sensor, kStreamResolution);
        cameraBusy_ = false;
        return false;
    }

    Serial.printf("Captured: %ux%u / Capturado: %ux%u\n", fb->width, fb->height);

    freeLatestCapture();

    latestCapture_ = static_cast<uint8_t*>(
        heap_caps_malloc(fb->len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    if (latestCapture_ == nullptr) {
        Serial.println("ERROR: Could not allocate capture buffer. / ERROR: no se pudo asignar el búfer.");
        esp_camera_fb_return(fb);
        sensor->set_framesize(sensor, kStreamResolution);
        cameraBusy_ = false;
        return false;
    }

    memcpy(latestCapture_, fb->buf, fb->len);
    latestCaptureSize_ = fb->len;
    latestCaptureId_++;

    Serial.printf("Stored capture #%lu / Captura guardada #%lu\n",
                  static_cast<unsigned long>(latestCaptureId_));

    esp_camera_fb_return(fb);

    sensor->set_framesize(sensor, kStreamResolution);
    delay(150);

    cameraBusy_ = false;
    return true;
}

// ---------------------------------------------------------------------------
// Capture / upload application flow (contracts preserved)
// ---------------------------------------------------------------------------

void Station::doCaptureAndUpload() {
    if (!captureHighResolution()) {
        return;  // capture failure IS the clear failure state (spec §8)
    }

    if (latestCapture_ == nullptr || latestCaptureSize_ == 0) {
        Serial.println("[EN] No capture available — nothing uploaded.");
        Serial.println("[ES] No hay captura — nada subido.");
        return;
    }

    if (armedEventId_ > 0) {
        // Card-first: classify the PRECEDING tap's event (spec §2/§8).
        Serial.printf("[EN] Card-first classify for event %ld...\n", armedEventId_);
        Serial.printf("[ES] Clasificación tarjeta-primero para el evento %ld...\n", armedEventId_);
        std::string body = CapturePayload::classifyWithEvent(
            armedEventId_, latestCapture_, latestCaptureSize_);
        HttpResponse r = api_.post("/api/v1/recycling/classify", body, CapturePayload::contentType());
        reportUpload("classify", r.status, String(r.body.c_str()), r.transportOk);
        armedEventId_ = -1;  // one-shot: never re-classify a stale event by accident
        return;
    }

    // Bottle-first (spec §3 Case B): image WITHOUT a student. The
    // backend holds it awaiting_card; ENTER never triggers
    // classification (the backend owns that decision — spec §4/§8).
    Serial.println("[EN] Bottle-first capture: uploading image (no card yet)...");
    Serial.println("[ES] Captura botella-primero: subiendo imagen (sin tarjeta aún)...");
    std::string body = CapturePayload::imageOnly(latestCapture_, latestCaptureSize_);
    HttpResponse r = api_.post("/api/v1/recycling/capture", body, CapturePayload::contentType());
    reportUpload("capture", r.status, String(r.body.c_str()), r.transportOk);

    backendCaptureId_ = extractLongField(String(r.body.c_str()), "capture_id");
    if (backendCaptureId_ > 0) {
        Serial.printf("[EN] Backend capture id %ld — now tap a card ('a <credential_uid>').\n", backendCaptureId_);
        Serial.printf("[ES] Captura %ld en el backend — ahora toca una tarjeta ('a <credential_uid>').\n", backendCaptureId_);
    }
}

void Station::doAssociate(const std::string& uid) {
    if (backendCaptureId_ <= 0) {
        Serial.println("[EN] No pending capture id — press ENTER first.");
        Serial.println("[ES] No hay captura pendiente — presiona ENTER primero.");
        return;
    }

    std::string body = buildAssociatePayload(uid);
    std::string path = "/api/v1/recycling/captures/" + std::to_string(backendCaptureId_) + "/associate";
    HttpResponse r = api_.post(path, body);
    reportUpload("associate", r.status, String(r.body.c_str()), r.transportOk);
}

void Station::handleCaptureCommand(const CaptureCommand& cmd) {
    switch (cmd.kind) {
        case CaptureCommand::Capture:
            doCaptureAndUpload();
            break;
        case CaptureCommand::Associate:
            doAssociate(cmd.arg);
            break;
        case CaptureCommand::ArmEvent:
            armedEventId_ = atol(cmd.arg.c_str());
            Serial.printf("[EN] Armed card-first classify for event %ld (next ENTER captures).\n", armedEventId_);
            Serial.printf("[ES] Armada clasificación tarjeta-primero para el evento %ld (el próximo ENTER captura).\n", armedEventId_);
            break;
        case CaptureCommand::LocalOnly:
            captureHighResolution();
            break;
        case CaptureCommand::None:
        default:
            Serial.println("[EN] Unknown command. Commands: ENTER=capture+upload | a <uid>=associate | e <event_id>=card-first | c=local capture");
            Serial.println("[ES] Comando desconocido. Comandos: ENTER=capturar+subir | a <uid>=asociar | e <evento>=tarjeta-primero | c=captura local");
            break;
    }
}

void Station::chirpSuccess() {
    if (PIN_CAM_BUZZER < 0) {
        return;  // absent on this bench: GPIO4 is RC522 RST
    }
    digitalWrite(PIN_CAM_BUZZER, HIGH);
    delay(120);
    digitalWrite(PIN_CAM_BUZZER, LOW);
}

void Station::reportUpload(const char* what, int status, const String& body, bool transportOk) {
    FeedbackSignal signal;
    if (transportOk && status == 200) {
        chirpSuccess();
        signal.kind = FeedbackKind::CaptureSuccess;
        led_.showEvent(signal);
    } else if (!transportOk) {
        signal.kind = FeedbackKind::NetworkError;
        led_.showEvent(signal);
    } else {
        signal.kind = FeedbackKind::ServerError;
        led_.showEvent(signal);
    }
    if (!transportOk) {
        Serial.printf("[EN] %s: NETWORK ERROR (transport). Check Wi-Fi / API_BASE_URL.\n", what);
        Serial.printf("[ES] %s: ERROR DE RED (transporte). Revisa Wi-Fi / API_BASE_URL.\n", what);
        return;
    }
    Serial.printf("[EN] %s: HTTP %ld\n", what, (long) status);
    Serial.printf("[ES] %s: HTTP %ld\n", what, (long) status);
    Serial.println(body);
}

// ---------------------------------------------------------------------------
// Serial: mode password + capture trigger share one terminal.
// Masked echo (never print typed secrets). Empty lines are console-Ignored
// and fall through to the capture trigger, so ENTER still captures — even
// during a password lockout. Non-empty wrong lines are console feedback
// only and never fire capture commands.
// ---------------------------------------------------------------------------

void Station::pollSerial() {
    const uint32_t now = millis();
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c != '\n' && c != '\r') {
            Serial.print('*');  // masked echo: never print what was typed
        }
        std::string line;
        if (lines_.feed(c, line)) {
            Serial.println();  // end the asterisk row on Enter
            if (lines_.overflowed()) {
                Serial.println("[MODE] input too long — line discarded /");
                Serial.println("     entrada demasiado larga — linea descartada");
            }
            dispatchSerialLine(line, now);
        }
    }
}

void Station::dispatchSerialLine(const std::string& line, uint32_t now) {
    if (line.empty()) {
        // Bare ENTER skips the console (Ignored by design) and goes
        // straight to the trigger — with its key-repeat cooldown.
        CaptureCommand cmd = trigger_.feed('\n');
        if (cmd.kind != CaptureCommand::None) {
            handleCaptureCommand(cmd);
        }
        return;
    }

    const ConsoleResult result = console_.handleLine(line, now);
    switch (result) {
        case ConsoleResult::Accepted:
            switchMode();
            return;  // a password is never a capture command
        case ConsoleResult::Rejected:
            Serial.println();
            Serial.print("[MODE] wrong password / clave incorrecta — ");
            if (console_.lockedOut(now)) {
                Serial.print("input locked for ");
                Serial.print(MODE_CONSOLE_LOCKOUT_MS / 1000);
                Serial.println(" s / entrada bloqueada");
            } else {
                Serial.print(MODE_CONSOLE_MAX_WRONG_ATTEMPTS - console_.wrongAttempts());
                Serial.println(" attempt(s) left / intento(s) restante(s)");
            }
            showModeEvent(FeedbackKind::ModeRejected);
            return;
        case ConsoleResult::LockedOut:
            Serial.println();
            Serial.println("[MODE] input locked — wait for the countdown /");
            Serial.println("     entrada bloqueada — espera la cuenta atras");
            showModeEvent(FeedbackKind::ModeRejected);
            return;
        case ConsoleResult::Ignored:
        default:
            break;
    }

    // Not a password attempt outcome — parse as a capture command line.
    for (char c : line) {
        CaptureCommand cmd = trigger_.feed(c);
        if (cmd.kind != CaptureCommand::None) {
            handleCaptureCommand(cmd);
        }
    }
    CaptureCommand cmd = trigger_.feed('\n');
    if (cmd.kind != CaptureCommand::None) {
        handleCaptureCommand(cmd);
    }
}

void Station::showModeEvent(FeedbackKind kind) {
    FeedbackSignal signal;
    signal.kind = kind;
    led_.showEvent(signal);
}

void Station::switchMode() {
    mode_ = (mode_->kind() == ModeKind::Pairing) ? static_cast<Mode*>(&operationMode_)
                                                : static_cast<Mode*>(&pairingMode_);
    Serial.println();
    Serial.print("[MODE] switched to / cambiado a: ");
    Serial.println(mode_->label());
    Serial.print("[MODE] ");
    Serial.print(mode_->hint());
    Serial.println();
    showModeEvent(FeedbackKind::ModeSwitched);
    refreshStateLed();
    debouncer_.reset();  // a tap in flight must not straddle the switch
}

// ---------------------------------------------------------------------------
// RFID → presence pipeline (reader logic, intact) + station transaction:
// a tap answered with next_step awaiting_classification auto-captures and
// classifies in the same flow (existing classifyWithEvent path).
// ---------------------------------------------------------------------------

void Station::printReaderKeyRemediation() {
    Serial.println("     READER_API_KEY has no matching reader row on the backend /");
    Serial.println("     READER_API_KEY no tiene una fila de lector en el backend —");
    Serial.println("     fix / arreglo: docs/PAIRING.md (provisioning / provisionamiento)");
}

void Station::handleCardTap(const std::string& uid) {
    Serial.println();
    Serial.print("[NFC] card / tarjeta: ");
    Serial.println(uid.c_str());

    ApiCall call = mode_->onCardTap(uid);
    HttpResponse response = api_.post(call.path, call.jsonBody);

    if (call.type == ApiCallType::Tap) {
        TapResult result = parseTapResponse(response.status, response.body);
        FeedbackSignal signal = operationMode_.interpret(result);

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
        led_.showEvent(signal);

        // Station transaction: identity resolved AND the backend wants a
        // photo for this event → capture + classify now (card-first).
        if (result.outcome == TapOutcome::Success && result.awaitingClassification &&
            result.eventId > 0) {
            Serial.printf("[STATION] auto-capture for event %ld / auto-captura para el evento %ld\n",
                          (long) result.eventId, (long) result.eventId);
            armedEventId_ = result.eventId;
            doCaptureAndUpload();  // consumes armedEventId_ (one-shot)
        }
    } else {  // ApiCallType::PairCard
        PairResult result = parsePairResponse(response.status, response.body);
        FeedbackSignal signal = pairingMode_.interpret(result);

        switch (result.outcome) {
            case PairOutcome::Success:
                Serial.print("[OK] card paired to / tarjeta emparejada con: ");
                Serial.println(result.pairedStudentName.c_str());
                break;
            case PairOutcome::NoActiveSession:
                Serial.print("[409] ");
                Serial.println(result.message.c_str());
                Serial.println("     arm a session first, then tap within the window —");
                Serial.println("     docs/PAIRING.md / arma primero una sesion y acerca");
                Serial.println("     la tarjeta dentro de la ventana — docs/PAIRING.md");
                break;
            case PairOutcome::AlreadyPaired:
                Serial.print("[422] ");
                Serial.println(result.message.c_str());
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
        led_.showEvent(signal);
    }
}

// ---------------------------------------------------------------------------
// Visualizer (reference routes, preserved)
// ---------------------------------------------------------------------------

void Station::setupRoutes() {
    server_.on("/", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/stream-frame", HTTP_GET, [this]() { handleStreamFrame(); });
    server_.on("/capture.jpg", HTTP_GET, [this]() { handleCaptureImage(); });
    server_.on("/capture-status", HTTP_GET, [this]() { handleCaptureStatus(); });
    server_.onNotFound([this]() {
        server_.send(404, "text/plain", "Not found / No encontrado");
    });
}

void Station::sendJPEG(const uint8_t* data, size_t length) {
    server_.setContentLength(length);
    server_.send(200, "image/jpeg", "");

    WiFiClient client = server_.client();

    size_t written = 0;
    while (written < length && client.connected()) {
        size_t chunk = length - written;
        if (chunk > 1460) {
            chunk = 1460;
        }
        size_t n = client.write(data + written, chunk);
        if (n == 0) {
            break;
        }
        written += n;
        delay(1);
    }
}

void Station::handleStreamFrame() {
    if (cameraBusy_) {
        server_.send(503, "text/plain", "Camera busy / Cámara ocupada");
        return;
    }
    if (!cameraOk_) {
        server_.send(503, "text/plain", "Camera down / Cámara caída");
        return;
    }

    camera_fb_t* fb = esp_camera_fb_get();
    if (fb == nullptr) {
        server_.send(500, "text/plain", "Camera capture failed / La captura falló");
        return;
    }

    sendJPEG(fb->buf, fb->len);
    esp_camera_fb_return(fb);
}

void Station::handleCaptureImage() {
    if (latestCapture_ == nullptr || latestCaptureSize_ == 0) {
        server_.send(404, "text/plain", "No capture available / No hay captura");
        return;
    }

    sendJPEG(latestCapture_, latestCaptureSize_);
}

void Station::handleCaptureStatus() {
    String json;
    json.reserve(192);
    json += "{\"id\":";
    json += String(latestCaptureId_);
    json += ",\"size\":";
    json += String(latestCaptureSize_);
    json += ",\"available\":";
    json += (latestCapture_ != nullptr ? "true" : "false");
    json += ",\"busy\":";
    json += (cameraBusy_ ? "true" : "false");
    json += ",\"camera\":";
    json += (cameraOk_ ? "\"ready\"" : "\"error\"");
    json += ",\"nfc\":";
    json += (nfc_.healthy() ? "\"ready\"" : "\"error\"");
    json += ",\"wifi\":";
    json += (wifi_.isConnected() ? "\"online\"" : "\"offline\"");
    json += "}";

    server_.send(200, "application/json", json);
}

void Station::handleRoot() {
    const char* html = R"rawliteral(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>Presence Station</title>
<style>
body{margin:0;padding:24px;background:#111;color:#eee;font-family:system-ui,sans-serif}
.container{width:min(1100px,100%);margin:auto}
h1{margin-top:0}
.section{margin-top:30px}
.image-box{background:#000;border-radius:10px;overflow:hidden}
.image-box img{display:block;width:100%;height:auto}
.placeholder{min-height:250px;display:flex;align-items:center;justify-content:center;color:#777;text-align:center}
.status{margin-top:12px;padding:10px;border-radius:6px;background:#222;font-family:monospace}
</style>
</head>
<body>
<div class="container">
<h1>Presence Platform — ESP32-CAM Station / Estación ESP32-CAM</h1>
<div class="section">
<h2>Live View</h2>
<div class="image-box"><img id="stream" alt="Live camera"></div>
</div>
<div class="section">
<h2>Latest High-Resolution Capture</h2>
<div class="image-box">
<div id="placeholder" class="placeholder">Tap a card or press ENTER in the serial terminal to capture &amp; upload. / Toca una tarjeta o presiona ENTER en el terminal serial para capturar y subir.</div>
<img id="capture" style="display:none" alt="High resolution capture">
</div>
<div id="status" class="status">Waiting... / Esperando...</div>
</div>
</div>
<script>
const stream=document.getElementById("stream");
async function updateStream(){try{const r=await fetch("/stream-frame?t="+Date.now(),{cache:"no-store"});if(!r.ok)throw new Error("HTTP "+r.status);const b=await r.blob();const u=URL.createObjectURL(b);stream.onload=()=>URL.revokeObjectURL(u);stream.src=u}catch(e){console.log("Stream error:",e)}setTimeout(updateStream,200)}
updateStream();
const capture=document.getElementById("capture");
const placeholder=document.getElementById("placeholder");
const status=document.getElementById("status");
let lastCaptureId=0;
async function checkCapture(){try{const r=await fetch("/capture-status?t="+Date.now(),{cache:"no-store"});const d=await r.json();if(d.available){if(d.id!==lastCaptureId){lastCaptureId=d.id;capture.src="/capture.jpg?t="+Date.now();capture.style.display="block";placeholder.style.display="none"}status.textContent="Capture #"+d.id+" | "+d.size+" bytes | cam:"+d.camera+" nfc:"+d.nfc+" wifi:"+d.wifi}else{status.textContent="No capture yet. cam:"+d.camera+" nfc:"+d.nfc+" wifi:"+d.wifi+" / Aún no hay capturas."}}catch(e){console.log("Capture status error:",e)}setTimeout(checkCapture,500)}
checkCapture();
</script>
</body>
</html>)rawliteral";

    server_.send(200, "text/html", html);
}

}  // namespace Presence
