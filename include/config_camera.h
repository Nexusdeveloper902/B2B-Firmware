/**
 * config_camera.h — CAMERA STATION copy of config.h. Started identical;
 * change freely per board: `esp32cam` builds read THIS file, the reader
 * envs (`esp32dev`, `esp32dev-mock`) read config.h. Neither affects the
 * other. / Copia para la CÁMARA: empezó idéntica, cámbiala por placa.
 *
 * (Original header kept below for reference.)
 * config.h — pin assignments and timing constants, separated per board.
 * config.h — asignación de pines y constantes de tiempo, por placa.
 *
 *   Section A  READER board (envs `esp32dev`, `esp32dev-mock`)
 *   Section B  CAMERA STATION board (env `esp32cam`)
 *   Section C  SHARED timing (both boards)
 *
 * ⚠ WIRING CONFIRMATION NOTE / NOTA DE CONFIRMACIÓN DE CABLEADO:
 * Every pin below must be confirmed against the physical wiring of YOUR
 * board before relying on it. See docs/HARDWARE_SETUP.md (+ .es.md) for
 * the reader and docs/CAMERA_STATION.md (+ .es.md) for the camera station.
 * Cada pin debe confirmarse contra el cableado físico de TU placa antes de
 * confiar en él.
 */
#pragma once

// ===========================================================================
// Section A — READER board (esp32dev, esp32dev-mock) / Placa LECTORA
// ===========================================================================
// Generic ESP32 DevKit using the VSPI bus for the RC522 and free GPIOs
// for LEDs/buzzer. / DevKit genérico con RC522 por SPI (VSPI).

// --- NFC reader: RC522 over SPI -------------------------------------------
#define PIN_RC522_SCK   14  // RC522 SCK  (VSPI clock)   — confirm wiring
#define PIN_RC522_MISO  2  // RC522 MISO (VSPI data in) — confirm wiring
#define PIN_RC522_MOSI  15  // RC522 MOSI (VSPI data out)— confirm wiring
#define PIN_RC522_SS    13   // RC522 SDA (SS)            — confirm wiring
#define PIN_RC522_RST   4  // RC522 RST                 — confirm wiring

// When the reader fails init (or dies at runtime: wiring glitch, ESD,
// brown-out), retry PCD_Init on this cadence instead of staying dead
// until a reboot. millis()-based, non-blocking.
// / Cadencia de reintento de PCD_Init si el lector falla (cableado, ESD,
// caída de tensión). Basado en millis(), no bloqueante.
#define RC522_REINIT_INTERVAL_MS  5000

// --- Mode switching: serial console password (TASK-003) -------------------
// The MODE PASSWORD VALUE lives in the gitignored include/secrets.h
// (MODE_PASSWORD — see secrets.h.example). These are the tunable knobs:
// / El VALOR de la clave vive en el gitignored include/secrets.h
// (MODE_PASSWORD — ver secrets.h.example). Estos son los parámetros:
#define MODE_CONSOLE_MAX_WRONG_ATTEMPTS  3     // wrong passwords before lockout
#define MODE_CONSOLE_LOCKOUT_MS           10000 // console lock after the wrongs
#define SERIAL_LINE_MAX_LENGTH            64    // serial input line cap (chars)

// --- Feedback hardware ----------------------------------------------------
// Two LEDs + an optional buzzer. LED_MODE blinks CONTINUOUSLY to show the
// current mode; LED_EVENT plays transient patterns for tap/pair results.
#define PIN_LED_MODE    25  // mode indicator LED — confirm wiring
#define PIN_LED_EVENT   26  // event feedback LED — confirm wiring
#define PIN_BUZZER      33  // optional passive buzzer; set to -1 if absent

// --- Reader-only timing (millis()-based, non-blocking) ---------------------
#define WIFI_RECONNECT_INTERVAL_MS     10000  // retry cadence after a drop
#define CARD_COOLDOWN_MS               2000   // same-UID re-read debounce
#define BOOT_SERIAL_WAIT_MS            2500   // wait for Serial Monitor attach

// ===========================================================================
// Section B — CAMERA STATION board (env `esp32cam`) / Placa CÁMARA
// ===========================================================================
// AI-Thinker ESP32-CAM (OV3660) with an RC522 aboard — the owner's bench
// wiring: RC522 3.3V → 3.3V | GND → GND | SDA/SS → GPIO13 | SCK → GPIO14 |
// MOSI → GPIO15 | MISO → GPIO2 | RST → GPIO4. Those six are RESERVED
// here (no PIN_ defines: this firmware doesn't drive them — yet).
// / Esos seis pines están RESERVADOS (sin defines: este firmware aún no
// los maneja).

// --- Camera bus pin map (AI-Thinker, from the verified reference) ---------
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

// --- Shutter button --------------------------------------------------------
// Momentary button between GPIO12 and GND, no external resistor
// (firmware enables the internal pull-up). Boot-safe: unpressed it
// floats LOW at reset (correct 3.3 V flash voltage); NEVER tie it HIGH
// (strapping pin). / Botón entre GPIO12 y GND, sin resistencia externa
// (pull-up interno).
#define PIN_SHUTTER_BUTTON  12   // active-LOW shutter — confirm wiring
#define SHUTTER_DEBOUNCE_MS 50   // contact-settle window per press

// --- Buzzer ----------------------------------------------------------------
// Buzzer (+) → GPIO4, (−) → GND; set to -1 if absent. Same part and
// same "success chirp only" convention as the reader's PIN_BUZZER.
// GPIO4 is the only free exposed pin (see the reserved lists above).
// Side effect: the white flash LED (also on 4) fires with each 120 ms
// chirp — harmless, free success flash.
// / Zumbador (+) → GPIO4, (−) → GND; -1 si no hay. El LED de flash (4)
// destella con cada pitido — normal, destello de éxito gratis.
#define PIN_CAM_BUZZER      -1    // active-HIGH chirp — confirm wiring

// ===========================================================================
// Section C — SHARED timing, both boards (millis()-based, non-blocking)
// Constantes de tiempo compartidas (todo no bloqueante, basado en millis())
// ===========================================================================
#define WIFI_CONNECT_TIMEOUT_MS        15000  // initial connect attempt window
#define HTTP_TIMEOUT_MS                10000  // per-request HTTP timeout
