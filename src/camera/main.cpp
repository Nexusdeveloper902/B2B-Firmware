/**
 * camera/main.cpp — the recycling station's CAMERA firmware (TASK-008).
 * camera/main.cpp — firmware de CÁMARA de la estación de reciclaje (TASK-008).
 *
 * Presence Platform — ESP32-CAM (AI-Thinker, OV3660), PlatformIO env
 * `esp32cam`. Merged from the known-working reference implementation
 * (ESP32-CAM-CV/firmware/esp32_cam, verified green 2026-09-07 by the
 * audit runs): camera init, JPEG capture, serial commands, and the HTTP
 * visualizer are preserved nearly verbatim. What TASK-008 adds is the
 * upload wiring to B2B-Core — the reference served images to a PC; this
 * firmware IS the image-acquisition half of the recycling flow.
 *
 * Wiring layer only: every byte of business logic (the trigger line
 * discipline, the multipart bodies, the Bearer value) lives in
 * host-tested PresenceCore — same three-way split as the reader.
 *
 * Serial commands (TerminalCaptureTrigger — the CaptureTrigger seam,
 * spec §37; the future IR sensor replaces ENTER at that seam only):
 *   ENTER            capture → POST /api/v1/recycling/capture
 *                    (bottle-first: the backend holds the image
 *                    awaiting_card — NO classifier call until a card
 *                    resolves it; spec §4 cost gate)
 *   a <credential_uid>  POST /api/v1/recycling/captures/<last>/associate
 *                    (the bottle-first resolution: card → event →
 *                    classify → points, B2B-Core TASK-025)
 *   e <event_id>     arm card-first mode: the NEXT ENTER captures and
 *                    POSTs /api/v1/recycling/classify with that event_id
 *   c                capture locally only (dev / visualizer; no upload)
 *
 * Bilingual: serial banner + status lines EN + ES (platform convention).
 */

#include <Arduino.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFi.h>

#include "esp_camera.h"
#include "esp_heap_caps.h"

#include <string>

#include "config.h"

// Real credentials live ONLY in the gitignored include/secrets.h — the
// same TASK-001 fresh-checkout guard the reader uses: a checkout without
// secrets.h still compiles against the placeholder values and fails
// Wi-Fi/HTTP gracefully instead of breaking the build.
// / Las credenciales reales viven SOLO en el gitignored include/secrets.h
// — la misma guarda de TASK-001: un checkout sin secrets.h compila con
// valores de marcador y falla Wi-Fi/HTTP con gracia.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "include/secrets.h not found — building with example placeholder values (cp include/secrets.h.example include/secrets.h). / No se encontró include/secrets.h — se compila con valores de ejemplo."
#include "secrets.h.example"
#endif

#include "CapturePayload.h"
#include "CaptureTrigger.h"
#include "CoreTypes.h"
#include "EspApiClient.h"
#include "PayloadBuilder.h"

using namespace Presence;

// ===========================================================================
// AI-Thinker ESP32-CAM pin map — from the reference platformio.ini/main
// (verified board: esp32cam). DO NOT invent a new map.
// ===========================================================================

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

static constexpr framesize_t STREAM_RESOLUTION = FRAMESIZE_VGA;
static constexpr framesize_t CAPTURE_RESOLUTION = FRAMESIZE_XGA;
static constexpr framesize_t INITIAL_FRAME_SIZE = FRAMESIZE_UXGA;

WebServer server(80);

// ===========================================================================
// Capture state (reference structure preserved)
// ===========================================================================

uint8_t *latestCapture = nullptr;
size_t latestCaptureSize = 0;
uint32_t latestCaptureId = 0;
volatile bool cameraBusy = false;

// TASK-008 upload state: the last capture the backend accepted
// (bottle-first) and the armed card-first event (if any).
long backendCaptureId = -1;
long armedEventId = -1;

// The JSON half of the wiring rides the SAME ApiClient the reader uses
// (EspApiClient: explicit Bearer header, bounded timeout, transport
// triage) — one auth path, TASK-007's lesson baked in.
static EspApiClient api(API_BASE_URL, READER_API_KEY, HTTP_TIMEOUT_MS);

// The trigger seam (spec §37): TerminalCaptureTrigger today, IR later —
// a drop-in replacement at THIS constructor only.
TerminalCaptureTrigger trigger([]() { return millis(); });

// ===========================================================================
// Free previous capture (reference, verbatim)
// ===========================================================================

void freeLatestCapture() {
    if (latestCapture != nullptr) {
        heap_caps_free(latestCapture);
        latestCapture = nullptr;
    }
    latestCaptureSize = 0;
}

// ===========================================================================
// HTTP upload — the TASK-008 wiring (thin: the bytes come from
// host-tested CapturePayload; the header VALUE from PresenceCore).
// ===========================================================================

// TASK-007 lesson, applied from day one: the Authorization header is
// built EXPLICITLY — never HTTPClient::setAuthorization(key), which
// prefixes the default type "Basic" and the backend ignores it.
struct UploadResult {
    bool transportOk = false;
    int status = 0;
    String body = "";
};

static UploadResult toUploadResult(const HttpResponse& r) {
    UploadResult out;
    out.transportOk = r.transportOk;
    out.status = r.status;
    out.body = String(r.body.c_str());
    return out;
}

UploadResult uploadMultipart(const String& path, const std::string& body) {
    UploadResult result;
    HTTPClient http;
    String url = String(API_BASE_URL) + path;

    if (!http.begin(url)) {
        return result;
    }
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.addHeader("Content-Type", CapturePayload::contentType().c_str());
    http.addHeader("Accept", "application/json");
    http.addHeader("Authorization",
                   bearerAuthorizationValue(READER_API_KEY).c_str());

    // HTTPClient's byte-array POST wants a mutable uint8_t* (legacy
    // Arduino signature); the payload bytes are sent verbatim.
    int code = http.POST(reinterpret_cast<uint8_t*>(const_cast<char*>(body.data())), body.size());

    result.status = code;
    if (code > 0) {
        result.transportOk = true;
        result.body = http.getString();
    }
    http.end();
    return result;
}

// Minimal field extraction from the backend JSON (ArduinoJson-free device
// trim: capture_id / event_id / points / state are printed raw; the exact
// response contracts are pinned by B2B-Core's own test suites).
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

void reportUpload(const char* what, const UploadResult& r) {
    if (!r.transportOk) {
        Serial.printf("[EN] %s: NETWORK ERROR (transport). Check Wi-Fi / API_BASE_URL.\n", what);
        Serial.printf("[ES] %s: ERROR DE RED (transporte). Revisa Wi-Fi / API_BASE_URL.\n", what);
        return;
    }
    Serial.printf("[EN] %s: HTTP %ld\n", what, (long) r.status);
    Serial.printf("[ES] %s: HTTP %ld\n", what, (long) r.status);
    Serial.println(r.body);
}

// ===========================================================================
// Capture high-resolution image (reference logic preserved; returns the
// buffer stays in latestCapture/latestCaptureSize)
// ===========================================================================

bool captureHighResolution() {
    if (cameraBusy) {
        Serial.println("Camera is already busy. / La cámara está ocupada.");
        return false;
    }

    cameraBusy = true;

    Serial.println();
    Serial.println("================================");
    Serial.println("HIGH-RES CAPTURE / CAPTURA ALTA RESOLUCIÓN");
    Serial.println("================================");

    sensor_t *sensor = esp_camera_sensor_get();

    if (sensor == nullptr) {
        Serial.println("ERROR: camera sensor unavailable. / ERROR: sensor no disponible.");
        cameraBusy = false;
        return false;
    }

    Serial.println("Switching sensor to XGA... / Cambiando sensor a XGA...");

    int result = sensor->set_framesize(sensor, CAPTURE_RESOLUTION);
    if (result != 0) {
        Serial.printf("ERROR: set_framesize failed: %d\n", result);
        sensor->set_framesize(sensor, STREAM_RESOLUTION);
        cameraBusy = false;
        return false;
    }

    delay(250);

    camera_fb_t *discard = esp_camera_fb_get();
    if (discard != nullptr) {
        esp_camera_fb_return(discard);
    } else {
        Serial.println("WARNING: first post-switch frame was unavailable. / AVISO: el primer frame tras el cambio no estaba disponible.");
    }

    delay(50);

    camera_fb_t *fb = esp_camera_fb_get();
    if (fb == nullptr) {
        Serial.println("ERROR: Camera capture failed. / ERROR: la captura falló.");
        sensor->set_framesize(sensor, STREAM_RESOLUTION);
        cameraBusy = false;
        return false;
    }

    Serial.printf("Captured: %ux%u / Capturado: %ux%u\n", fb->width, fb->height);

    freeLatestCapture();

    latestCapture = static_cast<uint8_t*>(
        heap_caps_malloc(fb->len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    if (latestCapture == nullptr) {
        Serial.println("ERROR: Could not allocate capture buffer. / ERROR: no se pudo asignar el búfer.");
        esp_camera_fb_return(fb);
        sensor->set_framesize(sensor, STREAM_RESOLUTION);
        cameraBusy = false;
        return false;
    }

    memcpy(latestCapture, fb->buf, fb->len);
    latestCaptureSize = fb->len;
    latestCaptureId++;

    Serial.printf("Stored capture #%lu / Captura guardada #%lu\n",
                  static_cast<unsigned long>(latestCaptureId));

    esp_camera_fb_return(fb);

    sensor->set_framesize(sensor, STREAM_RESOLUTION);
    delay(150);

    cameraBusy = false;
    return true;
}

// ===========================================================================
// TASK-008 flow: ENTER → capture → upload (bottle-first), or with an
// armed event id → classify (card-first). 'c' captures without upload.
// ===========================================================================

void doCaptureAndUpload() {
    if (!captureHighResolution()) {
        return;  // capture failure IS the clear failure state (spec §8)
    }

    if (latestCapture == nullptr || latestCaptureSize == 0) {
        Serial.println("[EN] No capture available — nothing uploaded.");
        Serial.println("[ES] No hay captura — nada subido.");
        return;
    }

    if (armedEventId > 0) {
        // Card-first: classify the PRECEDING tap's event (spec §2/§8).
        Serial.printf("[EN] Card-first classify for event %ld...\n", armedEventId);
        Serial.printf("[ES] Clasificación tarjeta-primero para el evento %ld...\n", armedEventId);
        std::string body = CapturePayload::classifyWithEvent(
            armedEventId, latestCapture, latestCaptureSize);
        UploadResult r = uploadMultipart("/api/v1/recycling/classify", body);
        reportUpload("classify", r);
        armedEventId = -1;  // one-shot: never re-classify a stale event by accident
        return;
    }

    // Bottle-first (spec §3 Case B): image WITHOUT a student. The
    // backend holds it awaiting_card; ENTER never triggers
    // classification (the backend owns that decision — spec §4/§8).
    Serial.println("[EN] Bottle-first capture: uploading image (no card yet)...");
    Serial.println("[ES] Captura botella-primero: subiendo imagen (sin tarjeta aún)...");
    std::string body = CapturePayload::imageOnly(latestCapture, latestCaptureSize);
    UploadResult r = uploadMultipart("/api/v1/recycling/capture", body);
    reportUpload("capture", r);

    backendCaptureId = extractLongField(r.body, "capture_id");
    if (backendCaptureId > 0) {
        Serial.printf("[EN] Backend capture id %ld — now tap a card ('a <credential_uid>').\n", backendCaptureId);
        Serial.printf("[ES] Captura %ld en el backend — ahora toca una tarjeta ('a <credential_uid>').\n", backendCaptureId);
    }
}

void doAssociate(const std::string& uid) {
    if (backendCaptureId <= 0) {
        Serial.println("[EN] No pending capture id — press ENTER first.");
        Serial.println("[ES] No hay captura pendiente — presiona ENTER primero.");
        return;
    }

    std::string body = buildAssociatePayload(uid);
    std::string path = "/api/v1/recycling/captures/" + std::to_string(backendCaptureId) + "/associate";
    reportUpload("associate", toUploadResult(api.post(path, body)));
}

void handleCaptureCommand(const CaptureCommand& cmd) {
    switch (cmd.kind) {
        case CaptureCommand::Capture:
            doCaptureAndUpload();
            break;
        case CaptureCommand::Associate:
            doAssociate(cmd.arg);
            break;
        case CaptureCommand::ArmEvent:
            armedEventId = atol(cmd.arg.c_str());
            Serial.printf("[EN] Armed card-first classify for event %ld (next ENTER captures).\n", armedEventId);
            Serial.printf("[ES] Armada clasificación tarjeta-primero para el evento %ld (el próximo ENTER captura).\n", armedEventId);
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

// ===========================================================================
// Serial handling — through the CaptureTrigger seam (line discipline +
// key-repeat cooldown live there, host-tested)
// ===========================================================================

void handleSerial() {
    while (Serial.available() > 0) {
        CaptureCommand cmd = trigger.feed(static_cast<char>(Serial.read()));
        if (cmd.kind != CaptureCommand::None) {
            handleCaptureCommand(cmd);
        }
    }
}

// ===========================================================================
// Visualizer (reference preserved: stream-frame / capture.jpg /
// capture-status / root) — the bench operator's eyes on the camera.
// ===========================================================================

void sendJPEG(const uint8_t *data, size_t length) {
    server.setContentLength(length);
    server.send(200, "image/jpeg", "");

    WiFiClient client = server.client();

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

void handleStreamFrame() {
    if (cameraBusy) {
        server.send(503, "text/plain", "Camera busy / Cámara ocupada");
        return;
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (fb == nullptr) {
        server.send(500, "text/plain", "Camera capture failed / La captura falló");
        return;
    }

    sendJPEG(fb->buf, fb->len);
    esp_camera_fb_return(fb);
}

void handleCaptureImage() {
    if (latestCapture == nullptr || latestCaptureSize == 0) {
        server.send(404, "text/plain", "No capture available / No hay captura");
        return;
    }

    sendJPEG(latestCapture, latestCaptureSize);
}

void handleCaptureStatus() {
    String json;
    json.reserve(128);
    json += "{\"id\":";
    json += String(latestCaptureId);
    json += ",\"size\":";
    json += String(latestCaptureSize);
    json += ",\"available\":";
    json += (latestCapture != nullptr ? "true" : "false");
    json += ",\"busy\":";
    json += (cameraBusy ? "true" : "false");
    json += "}";

    server.send(200, "application/json", json);
}

void handleRoot() {
    const char *html = R"rawliteral(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>Presence Camera Station</title>
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
<h1>Presence Platform — Camera Station / Estación de Cámara</h1>
<div class="section">
<h2>Live View</h2>
<div class="image-box"><img id="stream" alt="Live camera"></div>
</div>
<div class="section">
<h2>Latest High-Resolution Capture</h2>
<div class="image-box">
<div id="placeholder" class="placeholder">Press ENTER in the serial terminal to capture &amp; upload. / Presiona ENTER en el terminal serial para capturar y subir.</div>
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
async function checkCapture(){try{const r=await fetch("/capture-status?t="+Date.now(),{cache:"no-store"});const d=await r.json();if(d.available){if(d.id!==lastCaptureId){lastCaptureId=d.id;capture.src="/capture.jpg?t="+Date.now();capture.style.display="block";placeholder.style.display="none"}status.textContent="Capture #"+d.id+" | "+d.size+" bytes"}else{status.textContent="No capture yet. / Aún no hay capturas."}}catch(e){console.log("Capture status error:",e)}setTimeout(checkCapture,500)}
checkCapture();
</script>
</body>
</html>)rawliteral";

    server.send(200, "text/html", html);
}

// ===========================================================================
// Camera initialization (reference, verbatim)
// ===========================================================================

bool initializeCamera() {
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
        config.frame_size = INITIAL_FRAME_SIZE;
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

    sensor_t *sensor = esp_camera_sensor_get();
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

    sensor->set_framesize(sensor, STREAM_RESOLUTION);
    Serial.println("Camera started at VGA. / Cámara iniciada en VGA.");
    return true;
}

// ===========================================================================
// Wi-Fi (bounded like the reader's WifiService)
// ===========================================================================

bool connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("[EN] Connecting to WiFi");
    Serial.print(" [ES] Conectando a WiFi");

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println();
            Serial.println("[EN] WiFi connect TIMEOUT — uploads will fail until reboot.");
            Serial.println("[ES] Tiempo de conexión agotado — las subidas fallarán hasta reiniciar.");
            return false;
        }
    }

    Serial.println();
    Serial.print("[EN] WiFi connected. IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("[ES] WiFi conectado. IP: ");
    Serial.println(WiFi.localIP());
    return true;
}

// ===========================================================================
// Setup / loop
// ===========================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("================================");
    Serial.println("PRESENCE PLATFORM — CAMERA STATION");
    Serial.println("ESTACIÓN DE CÁMARA — PRESENCE PLATFORM");
    Serial.println("(TASK-008: ESP32-CAM merge, AI-Thinker/OV3660)");
    Serial.println("================================");

    // Guard against flashing the unmodified example secrets.
    if (strstr(WIFI_SSID, "YOUR_") != nullptr ||
        strstr(WIFI_PASSWORD, "YOUR_") != nullptr ||
        strstr(READER_API_KEY, "00000000000000000000000000000000") != nullptr) {
        Serial.println("[EN] WARNING: secrets.h still contains placeholder values —");
        Serial.println("     edit include/secrets.h (WIFI_SSID, WIFI_PASSWORD, API_BASE_URL, READER_API_KEY).");
        Serial.println("[ES] AVISO: secrets.h aún tiene valores de marcador —");
        Serial.println("     edita include/secrets.h (WIFI_SSID, WIFI_PASSWORD, API_BASE_URL, READER_API_KEY).");
    }

    if (!initializeCamera()) {
        Serial.println("FATAL: Camera initialization failed. / FATAL: falló la inicialización de la cámara.");
        while (true) {
            delay(1000);
        }
    }

    connectWiFi();

    server.on("/", HTTP_GET, handleRoot);
    server.on("/stream-frame", HTTP_GET, handleStreamFrame);
    server.on("/capture.jpg", HTTP_GET, handleCaptureImage);
    server.on("/capture-status", HTTP_GET, handleCaptureStatus);
    server.onNotFound([]() {
        server.send(404, "text/plain", "Not found / No encontrado");
    });
    server.begin();

    Serial.println();
    Serial.println("================================");
    Serial.println("SERVER READY / SERVIDOR LISTO");
    Serial.println("================================");
    Serial.print("[EN] Visualizer: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/");
    Serial.println();
    Serial.println("[EN] Serial commands / [ES] Comandos seriales:");
    Serial.println("  ENTER            capture + upload (bottle-first) / capturar + subir (botella-primero)");
    Serial.println("  a <credential_uid>  associate last capture with this card / asociar la última captura con esta tarjeta");
    Serial.println("  e <event_id>     arm card-first classify for next ENTER / armar clasificación tarjeta-primero para el próximo ENTER");
    Serial.println("  c                local capture only (no upload) / captura local sin subir");
    Serial.println();
}

void loop() {
    server.handleClient();
    handleSerial();
    delay(1);
}
