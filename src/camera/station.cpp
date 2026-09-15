/**
 * station.cpp — Station lifecycle + subsystem integration.
 * See station.h for the architecture. Code below is MOVED (not rewritten)
 * from src/main.cpp (tap pipeline, console) and the former camera-only
 * src/camera/main.cpp (capture, upload, visualizer), with two behavior
 * changes: (1) camera/Wi-Fi failures are recoverable states, never halts;
 * (2) tap is context-sensitive: with a bottle-first capture pending it
 * associates that capture (no new event); otherwise a tap answered with
 * next_step awaiting_classification ARMS the event — capture+classify on
 * the NEXT button/ENTER press (no auto-capture).
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
    // Needle tolerates optional whitespace after the colon: a backend
    // that pretty-prints (or adds a space in a refactor) must not silently
    // break the capture-id extraction into a dead tap-to-associate flow.
    String needle = String("\"") + field + "\":";
    int at = body.indexOf(needle);
    if (at < 0) {
        return -1;
    }
    at += needle.length();
    while (at < (int) body.length() && (body[(unsigned) at] == ' ' || body[(unsigned) at] == '\t')) {
        at++;
    }
    String digits;
    while (at < (int) body.length()) {
        const unsigned char ch = (unsigned char) body[(unsigned) at];
        if (isdigit(ch) || ch == '-') {
            digits += (char) ch;
            at++;
        } else {
            break;
        }
    }
    return digits.length() ? digits.toInt() : -1;
}

}  // namespace

Station::Station()
    : server_(80),
      // TASK-010 (LED follow-up): polarity comes from the board config —
      // one define flips inverted-polarity clone boards (see
      // include/config/esp32cam.h + the diagnostics comment there).
      led_(PIN_STATION_LED, PIN_STATION_LED_ACTIVE_LOW != 0),
      wifi_(WIFI_SSID, WIFI_PASSWORD, WIFI_CONNECT_TIMEOUT_MS, WIFI_RECONNECT_INTERVAL_MS),
      api_(API_BASE_URL, READER_API_KEY, HTTP_TIMEOUT_MS),
      nfc_(PIN_RC522_SS, PIN_RC522_RST, PIN_RC522_SCK, PIN_RC522_MISO, PIN_RC522_MOSI, &tsLog_),
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
        strstr(READER_API_KEY, "00000000000000000000000000000000") != nullptr ||
        strstr(API_BASE_URL, "localhost") != nullptr ||
        strstr(API_BASE_URL, "127.0.0.1") != nullptr) {
        Serial.println("[EN] WARNING: secrets.camera.h still contains placeholder values —");
        Serial.println("     edit include/secrets.camera.h (WIFI_SSID, WIFI_PASSWORD, API_BASE_URL, READER_API_KEY).");
        Serial.println("     API_BASE_URL must be the BACKEND's LAN address — 'localhost' on the");
        Serial.println("     ESP32 means the board itself, and every request would fail.");
        Serial.println("[ES] AVISO: secrets.camera.h aún tiene valores de marcador —");
        Serial.println("     edita include/secrets.camera.h (WIFI_SSID, WIFI_PASSWORD, API_BASE_URL, READER_API_KEY).");
        Serial.println("     API_BASE_URL debe ser la dirección LAN del BACKEND — 'localhost' en el");
        Serial.println("     ESP32 es la propia placa, y cada petición fallaría.");
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

    discoverPulseServer();  // TASK-013: DNS-SD first, compiled fallback kept

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
    Serial.print("Build: ");
    Serial.println(PULSE_FW_BUILD);  // bench rule: every flashed change bumps this
    Serial.println("================================");
    Serial.print("[EN] Visualizer / [ES] Visualizador: http://");
    Serial.print(wifi_.ip().c_str());
    Serial.println("/");
    Serial.print("Reader impl / Implementacion: ");
    Serial.println(nfc_.label());
    Serial.print("Mode / Modo: ");
    Serial.println(mode_->label());
    Serial.print("Backend: ");
    Serial.println(api_.baseUrl().c_str());  // effective URL: discovered, or the compiled fallback
    Serial.println();
    Serial.println("[EN] Serial commands / [ES] Comandos seriales:");
    Serial.println("  TAP CARD         closes pending capture (associate) — else arms card-first, auto-captures after a delay (BUTTON/ENTER now) / tocar tarjeta cierra captura pendiente — si no, arma y auto-captura tras la espera (BOTÓN/ENTER ya)");
    Serial.println("  ENTER            capture + upload (bottle-first if nothing armed) / capturar + subir (botella-primero si nada armado)");
    Serial.println("  a <credential_uid>  associate last capture with this card / asociar la última captura con esta tarjeta");
    Serial.println("  e <event_id>     arm card-first classify (same auto-capture) / armar clasificación tarjeta-primero (igual auto-captura)");
    Serial.println("  c                local capture only (no upload) / captura local sin subir");
    Serial.println("  BUTTON (GPIO12->GND) same as ENTER / igual que ENTER");
    Serial.println("  MODE PASSWORD + Enter switches operation/pairing (masked, lockout-guarded) /");
    Serial.println("  CONTRASEÑA DE MODO + Enter cambia operación/emparejamiento (enmascarada, con bloqueo)");
    Serial.printf("  Windows: tap-after-capture %lus, button-after-tap %lus / Ventanas: toque-tras-captura %lus, botón-tras-toque %lus\n",
                  (unsigned long) (PENDING_CAPTURE_TIMEOUT_MS / 1000),
                  (unsigned long) (ARMED_EVENT_TIMEOUT_MS / 1000),
                  (unsigned long) (PENDING_CAPTURE_TIMEOUT_MS / 1000),
                  (unsigned long) (ARMED_EVENT_TIMEOUT_MS / 1000));
    Serial.println();
}

// ---------------------------------------------------------------------------
// update()
// ---------------------------------------------------------------------------

void Station::update() {
    const uint32_t now = millis();

    wifi_.tick(now);
    discovery_.begin("pulse-station");  // late Wi-Fi: start mDNS once it associates
    led_.tick(now);
    server_.handleClient();
    expireStaleTransactions(now);  // anti-steal windows, before any tap/button use
    pollSerial();

    if (shutter_.poll()) {
        handleCaptureCommand({CaptureCommand::Capture, ""});  // button == ENTER
    }

    // New flow: tap → wait → auto photo. One shot per arm — a failed capture
    // keeps the arm so BUTTON/ENTER can still retry it manually.
    if (armedEventId_ > 0 && !autoCaptureDone_ &&
        (now - armedAtMs_) >= CARD_FIRST_AUTO_CAPTURE_DELAY_MS) {
        autoCaptureDone_ = true;
        handleCaptureCommand({CaptureCommand::Capture, ""});
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
            cameraFailures_ = 0;
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
        flagCameraFailure();
        return false;
    }

    cameraFailures_ = 0;  // a good frame clears the degraded suspicion

    Serial.printf("Captured: %ux%u / Capturado: %ux%u\n", fb->width, fb->height,
                  fb->width, fb->height);

    freeLatestCapture();

    // Capture buffer: PSRAM first, DRAM fallback. The camera init has an
    // explicit no-PSRAM path (QVGA/DRAM), so a PSRAM-less board boots and
    // streams — hard-requiring SPIRAM here would leave it permanently
    // unable to capture ("could not allocate" on every ENTER).
    latestCapture_ = static_cast<uint8_t*>(
        heap_caps_malloc(fb->len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (latestCapture_ == nullptr) {
        latestCapture_ = static_cast<uint8_t*>(heap_caps_malloc(fb->len, MALLOC_CAP_8BIT));
    }

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
                  static_cast<unsigned long>(latestCaptureId_),
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

void Station::flagCameraFailure() {
    // Boot-time init is only half of "never FATAL-halt": the camera can
    // also DIE at runtime (cable glitch, sensor lock-up) — fb_get keeps
    // returning nullptr forever while cameraOk_ stayed true and the
    // re-init cadence never engaged. Consecutive failures flip the flag,
    // so update()'s existing recovery loop takes over.
    cameraFailures_++;
    if (cameraOk_ && cameraFailures_ >= CAMERA_FAILURES_BEFORE_REINIT) {
        cameraOk_ = false;
        Serial.printf("[CAM] %d consecutive capture failures — periodic re-init engaged / fallos consecutivos — reintento periódico activado\n",
                      CAMERA_FAILURES_BEFORE_REINIT);
    }
}

// TASK-013: boot discovery (bounded, then the compiled fallback) plus the
// single runtime choke point — a transport failure re-queries _pulse._tcp
// (cooldown-guarded) and re-points the HTTP layer, so a DHCP-rotated
// backend is picked up with no reboot. The failed POST is NOT retried.
void Station::discoverPulseServer() {
    discovery_.begin("pulse-station");
    if (!wifi_.isConnected()) {
        return;  // offline: the fallback stays until Wi-Fi is up
    }
    for (int attempt = 0; attempt < PULSE_DISCOVERY_BOOT_ATTEMPTS; ++attempt) {
        const PulseEndpoint endpoint = discovery_.discover();
        if (endpoint.valid) {
            api_.setBaseUrl(endpoint.baseUrl());
            Serial.print("[DISC] Pulse backend / backend Pulse: ");
            Serial.println(endpoint.baseUrl().c_str());
            return;
        }
    }
    Serial.print("[DISC] no Pulse service (" PULSE_MDNS_LABEL ") — fallback / ");
    Serial.print("sin servicio Pulse (" PULSE_MDNS_LABEL ") — reserva: ");
    Serial.println(api_.baseUrl().c_str());
}

HttpResponse Station::post(const std::string& path, const std::string& body,
                           const std::string& contentType) {
    HttpResponse response = !contentType.empty()
                                ? api_.post(path, body, contentType)
                                : api_.post(path, body);
    if (!response.transportOk) {
        PulseEndpoint endpoint;
        if (discovery_.refreshIfDue(millis(), PULSE_REDISCOVER_COOLDOWN_MS,
                                    endpoint) &&
            endpoint.valid) {
            api_.setBaseUrl(endpoint.baseUrl());
            Serial.print("[DISC] backend re-discovered / backend redescubierto: ");
            Serial.println(endpoint.baseUrl().c_str());
        }
    }
    return response;
}

HttpResponse Station::postMultipart(const std::string& path, const std::string& body,
                                    const std::string& contentType,
                                    const std::string& signingBody) {
    HttpResponse response = api_.postMultipart(path, body, contentType, signingBody);
    if (!response.transportOk) {
        PulseEndpoint endpoint;
        if (discovery_.refreshIfDue(millis(), PULSE_REDISCOVER_COOLDOWN_MS,
                                    endpoint) &&
            endpoint.valid) {
            api_.setBaseUrl(endpoint.baseUrl());
            Serial.print("[DISC] backend re-discovered / backend redescubierto: ");
            Serial.println(endpoint.baseUrl().c_str());
        }
    }
    return response;
}

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
        // Lazy expiry first: a stale arm must never steal the new bottle.
        expireStaleTransactions(millis());
        if (armedEventId_ <= 0) {
            // Expired just now — fall through to bottle-first below.
        } else {
            Serial.printf("[EN] Card-first classify for event %ld...\n", armedEventId_);
            Serial.printf("[ES] Clasificación tarjeta-primero para el evento %ld...\n", armedEventId_);
            std::string body = CapturePayload::classifyWithEvent(
                armedEventId_, latestCapture_, latestCaptureSize_);
            // Multipart canonical signing: PHP reconstructs the same string
            // from the parsed upload (raw multipart never reaches it).
            std::string signing = CapturePayload::classifySigningBody(
                armedEventId_, latestCapture_, latestCaptureSize_);
            HttpResponse r = postMultipart("/api/v1/recycling/classify", body,
                                           CapturePayload::contentType(), signing);
            reportUpload("classify", r.status, String(r.body.c_str()), r.transportOk);
            if (r.transportOk && r.status == 200) {
                armedEventId_ = -1;  // one-shot on SUCCESS only: never re-classify a stale event by accident
            } else {
                // Upload failure (Wi-Fi drop, backend down, 5xx): the arm
                // STAYS (station.h contract — failures stay BUTTON/ENTER
                // retryable), bounded by ARMED_EVENT_TIMEOUT_MS. The event
                // is still awaiting_classification on the backend, so a
                // retry is exactly the right next move.
                Serial.println("[STATION] classify upload failed — arm kept, press BUTTON/ENTER to retry / subida fallida — brazo conservado, BOTÓN/ENTER reintenta");
            }
            return;
        }
    }

    // Bottle-first (spec §3 Case B): image WITHOUT a student. The
    // backend holds it awaiting_card; ENTER never triggers
    // classification (the backend owns that decision — spec §4/§8).
    Serial.println("[EN] Bottle-first capture: uploading image (no card yet)...");
    Serial.println("[ES] Captura botella-primero: subiendo imagen (sin tarjeta aún)...");
    std::string body = CapturePayload::imageOnly(latestCapture_, latestCaptureSize_);
    std::string signing = CapturePayload::captureSigningBody(latestCapture_, latestCaptureSize_);
    HttpResponse r = postMultipart("/api/v1/recycling/capture", body,
                                   CapturePayload::contentType(), signing);
    reportUpload("capture", r.status, String(r.body.c_str()), r.transportOk);

    if (!r.transportOk || r.status != 200) {
        return;  // keep any previous pending id so the operator can retry
    }
    const long newId = extractLongField(String(r.body.c_str()), "capture_id");
    if (newId > 0) {
        if (backendCaptureId_ > 0 && backendCaptureId_ != newId) {
            Serial.printf("[STATION] overwriting pending capture %ld with %ld (old one expires server-side) / reemplazando captura pendiente %ld con %ld\n",
                          (long) backendCaptureId_, newId, (long) backendCaptureId_, newId);
        }
        backendCaptureId_ = newId;
        backendCaptureAtMs_ = millis();
        Serial.printf("[EN] Backend capture id %ld — tap a card within %lus (or type 'a <credential_uid>').\n",
                      backendCaptureId_, (unsigned long) (PENDING_CAPTURE_TIMEOUT_MS / 1000));
        Serial.printf("[ES] Captura %ld en el backend — toca una tarjeta dentro de %lus (o escribe 'a <credential_uid>').\n",
                      backendCaptureId_, (unsigned long) (PENDING_CAPTURE_TIMEOUT_MS / 1000));
    }
}

void Station::doAssociate(const std::string& uid) {
    expireStaleTransactions(millis());
    if (backendCaptureId_ <= 0) {
        Serial.println("[EN] No pending capture id — press ENTER first.");
        Serial.println("[ES] No hay captura pendiente — presiona ENTER primero.");
        return;
    }

    const long target = backendCaptureId_;
    std::string body = buildAssociatePayload(uid);
    std::string path = "/api/v1/recycling/captures/" + std::to_string(target) + "/associate";
    HttpResponse r = post(path, body);
    reportUpload("associate", r.status, String(r.body.c_str()), r.transportOk);
    if (r.transportOk && r.status == 200) {
        backendCaptureId_ = -1;  // resolved — next tap arms a new card-first
    } else if (r.transportOk && r.status == 404) {
        // A 404 is NOT always "capture gone": the backend also 404s on an
        // unknown/inactive CARD (mistyped 'a <uid>'), and that must keep
        // the pending capture alive — the window stays waiting for the
        // right card (backend spec §32). Clear the pending id only when
        // the capture itself is gone/expired or already resolved; any
        // other 404 keeps it (bounded by PENDING_CAPTURE_TIMEOUT_MS).
        const bool captureGone = r.body.find("no_pending_capture") != std::string::npos;
        const bool alreadyResolved = r.body.find("already_associated") != std::string::npos;
        if (captureGone || alreadyResolved) {
            Serial.printf("[STATION] pending capture %ld gone/expired — cleared / captura %ld ausente/expirada — liberada\n",
                          target, target);
            backendCaptureId_ = -1;
        } else {
            Serial.printf("[STATION] capture %ld kept — card rejected, tap the right card / captura %ld conservada — tarjeta rechazada, toca la tarjeta correcta\n",
                          target, target);
        }
    }
    // transport/network errors keep the pending id so the operator can retry
}

void Station::expireStaleTransactions(uint32_t now) {
    // Wrap-safe: unsigned subtraction handles the ~49.7 day millis() wrap.
    if (backendCaptureId_ > 0 &&
        (now - backendCaptureAtMs_) >= PENDING_CAPTURE_TIMEOUT_MS) {
        Serial.printf("[STATION] pending capture %ld expired after %lus — cleared, next tap starts a new card-first / captura %ld expirada tras %lus — liberada\n",
                      (long) backendCaptureId_, (unsigned long) (PENDING_CAPTURE_TIMEOUT_MS / 1000),
                      (long) backendCaptureId_, (unsigned long) (PENDING_CAPTURE_TIMEOUT_MS / 1000));
        backendCaptureId_ = -1;
    }
    if (armedEventId_ > 0 && (now - armedAtMs_) >= ARMED_EVENT_TIMEOUT_MS) {
        Serial.printf("[STATION] armed event %ld expired after %lus — cleared, button is bottle-first again / evento armado %ld expirado tras %lus — liberado\n",
                      (long) armedEventId_, (unsigned long) (ARMED_EVENT_TIMEOUT_MS / 1000),
                      (long) armedEventId_, (unsigned long) (ARMED_EVENT_TIMEOUT_MS / 1000));
        armedEventId_ = -1;
    }
}

void Station::handleCaptureCommand(const CaptureCommand& cmd) {
    expireStaleTransactions(millis());
    switch (cmd.kind) {
        case CaptureCommand::Capture:
            doCaptureAndUpload();
            break;
        case CaptureCommand::Associate:
            doAssociate(cmd.arg);
            break;
        case CaptureCommand::ArmEvent:
            // The trigger accepts digits-only strings — including "0" and
            // 60-digit monsters that atol clamps. Event ids are small
            // positive integers; anything else is a typo and must be
            // REFUSED, or the operator waits forever for a photo that no
            // armed state will ever produce (every consumer checks > 0).
            if (cmd.arg.size() > 9 || atol(cmd.arg.c_str()) <= 0) {
                Serial.println("[EN] Invalid event id — a card-first arm needs a positive event id from a tap log.");
                Serial.println("[ES] Id de evento inválido — el modo tarjeta-primero necesita un id positivo del log.");
                return;
            }
            armedEventId_ = atol(cmd.arg.c_str());
            armedAtMs_ = millis();
            autoCaptureDone_ = false;
            Serial.printf("[EN] Armed card-first classify for event %ld (auto-captures in %lus, BUTTON/ENTER now).\n",
                          armedEventId_, (unsigned long) (CARD_FIRST_AUTO_CAPTURE_DELAY_MS / 1000));
            Serial.printf("[ES] Armada clasificación tarjeta-primero para el evento %ld (auto-captura en %lus, BOTÓN/ENTER ya).\n",
                          armedEventId_, (unsigned long) (CARD_FIRST_AUTO_CAPTURE_DELAY_MS / 1000));
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
        return;  // absent on this bench (no free pin; GPIO4 is the flash LED)
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
// only and never fire capture commands. A CRLF pair counts as ONE Enter
// (pollSerial drops the \n after a \r) — otherwise every typed line +
// Enter would ghost-fire a capture on its own tail. ENTER captures only
// when the line is otherwise empty.
// ---------------------------------------------------------------------------

void Station::pollSerial() {
    const uint32_t now = millis();
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\n' && prevSerialWasCR_) {
            // CRLF tail, not a second ENTER — the \r already completed
            // the line (PlatformIO/Arduino monitors send \r\n per Enter).
            prevSerialWasCR_ = false;
            continue;
        }
        prevSerialWasCR_ = (c == '\r');
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

    // COMMAND GRAMMAR FIRST: a command-shaped line (c / a <uid> /
    // e <id>) is dispatched as a capture command; the trigger answers
    // None for anything else (the mode password included), and only
    // then does the password console judge the line. The previous order
    // (console first) made ModeConsole treat EVERY non-empty line as a
    // password attempt — the documented a/e/c commands printed "wrong
    // password", poisoned the lockout counter, and never reached the
    // parser below. Ambiguity note: a mode password that collides with
    // the command grammar (e.g. literally "c") is consumed as the
    // command — pick a password outside those shapes.
    CaptureCommand cmd = trigger_.feedLine(line);
    if (cmd.kind != CaptureCommand::None) {
        handleCaptureCommand(cmd);
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
            return;  // nothing else can claim a non-empty line
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
    backendCaptureId_ = -1;  // pending bottle must not cross modes (anti-steal)
    armedEventId_ = -1;      // armed card-first must not cross modes
}

// ---------------------------------------------------------------------------
// RFID → presence pipeline (reader logic, intact) + station transaction:
// tap closes a pending bottle-first capture (associate), else a tap with
// next_step awaiting_classification ARMS the event for the NEXT
// button/ENTER press (classifyWithEvent). No auto-capture.
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

    // Same kind contract as the reader main: "hce" (phone authenticated
    // through the APDU exchange) reaches pairing as credential_kind; the
    // tap/associate lookups stay credential_uid-only (RF UID ≠ identity).
    ApiCall call = mode_->onCardTap(uid, nfc_.lastKind());
    // Lazy expiry first: an expired pending must not steal this tap.
    expireStaleTransactions(millis());
    // Bottle-first pending: a physical tap IS the associate (no new tap
    // event, no arming). Serial 'a <uid>' does the same via doAssociate.
    if (call.type == ApiCallType::Tap && backendCaptureId_ > 0) {
        Serial.printf("[STATION] tap closes pending capture %ld — associating, no new event / el toque cierra la captura pendiente %ld — asociando\n",
                      (long) backendCaptureId_, (long) backendCaptureId_);
        doAssociate(uid);
        return;
    }
    HttpResponse response = post(call.path, call.jsonBody);

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

        // Station transaction (card-first, auto): identity resolved AND
        // the backend wants a photo for this event → ARM it. update()
        // auto-captures+classifies once the delay elapses
        // (doCaptureAndUpload consumes armedEventId_ one-shot);
        // BUTTON/ENTER captures immediately without waiting.
        if (result.outcome == TapOutcome::Success && result.awaitingClassification &&
            result.eventId > 0) {
            // No bottle pending here (early-return above associates it), so
            // this tap is a pure card-first arm.
            if (armedEventId_ == result.eventId) {
                // Re-tap of the SAME event (backend dedup, or a retry after
                // a failed upload): keep the current arm exactly as it is.
                // Re-arming would reset autoCaptureDone_ and re-classify an
                // event that may already be classified.
                Serial.printf("[STATION] event %ld already armed — capture pending, no re-arm / evento %ld ya armado — captura pendiente, sin rearme\n",
                              (long) result.eventId, (long) result.eventId);
                return;
            }
            if (armedEventId_ > 0) {
                Serial.printf("[STATION] overwriting armed event %ld with %ld (the old one expires server-side unclassified) / reemplazando evento armado %ld con %ld\n",
                              (long) armedEventId_, (long) result.eventId,
                              (long) armedEventId_, (long) result.eventId);
            }
            armedEventId_ = result.eventId;
            armedAtMs_ = millis();
            autoCaptureDone_ = false;
            Serial.printf("[STATION] card-first armed for event %ld — auto-capture in %lus (BUTTON/ENTER now) / armado para el evento %ld — auto-captura en %lus (BOTÓN/ENTER ya)\n",
                          (long) result.eventId, (unsigned long) (CARD_FIRST_AUTO_CAPTURE_DELAY_MS / 1000),
                          (long) result.eventId, (unsigned long) (CARD_FIRST_AUTO_CAPTURE_DELAY_MS / 1000));
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
<title>Pulse Station</title>
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
<h1>Pulse — ESP32-CAM Station / Estación ESP32-CAM</h1>
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
