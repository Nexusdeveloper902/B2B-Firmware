/**
 * config.h — pin assignments and timing constants
 * config.h — asignación de pines y constantes de tiempo
 *
 * Presence Platform — Reader Firmware (ESP32)
 *
 * ⚠ WIRING CONFIRMATION NOTE / NOTA DE CONFIRMACIÓN DE CABLEADO:
 * Every pin below must be confirmed against the physical wiring of YOUR
 * board before relying on it. See docs/HARDWARE_SETUP.md (+ .es.md) for the
 * full wiring table and how to change these values.
 * Cada pin debe confirmarse contra el cableado físico de TU placa antes de
 * confiar en él. Consulta docs/HARDWARE_SETUP.md para la tabla completa.
 *
 * Defaults assume a generic ESP32 DevKit (esp32dev) using the VSPI bus
 * for the RC522 and free GPIOs for LEDs/button/buzzer.
 */
#pragma once

// ---------------------------------------------------------------------------
// NFC reader — RC522 over SPI / Lector NFC — RC522 por SPI
// ---------------------------------------------------------------------------
// ESP32 hardware SPI (VSPI) default pins — do not normally change:
//   SCK  = GPIO18, MISO = GPIO19, MOSI = GPIO23
#define PIN_RC522_SS    5   // RC522 SDA (SS)  — confirm against wiring
#define PIN_RC522_RST   27  // RC522 RST       — confirm against wiring

// ---------------------------------------------------------------------------
// Mode-select button (read once at boot) / Botón de selección de modo
// ---------------------------------------------------------------------------
// Wired to GND through a momentary button; internal pull-up enabled.
//   Pin state at boot      → Mode / Estado del pin al arrancar → Modo
//   HIGH (button released) → OPERATION MODE  (normal taps)
//   LOW  (button pressed)  → PAIRING MODE    (pair a new card)
#define PIN_MODE_SELECT      32  // GPIO used, INPUT_PULLUP — confirm wiring
#define MODE_LEVEL_PAIRING   LOW // logic level that selects PAIRING at boot

// ---------------------------------------------------------------------------
// Feedback hardware / Hardware de retroalimentación
// ---------------------------------------------------------------------------
// Two LEDs + an optional buzzer. LED_MODE blinks CONTINUOUSLY to show the
// current mode; LED_EVENT plays transient patterns for tap/pair results.
#define PIN_LED_MODE    25  // mode indicator LED — confirm wiring
#define PIN_LED_EVENT   26  // event feedback LED — confirm wiring
#define PIN_BUZZER      33  // optional passive buzzer; set to -1 if absent

// ---------------------------------------------------------------------------
// Timing constants (all non-blocking, millis()-based)
// Constantes de tiempo (todo no bloqueante, basado en millis())
// ---------------------------------------------------------------------------
#define WIFI_CONNECT_TIMEOUT_MS        15000  // initial connect attempt window
#define WIFI_RECONNECT_INTERVAL_MS     10000  // retry cadence after a drop
#define HTTP_TIMEOUT_MS                10000  // per-request HTTP timeout
#define CARD_COOLDOWN_MS               2000   // same-UID re-read debounce
#define MODE_BUTTON_SETTLE_MS          50     // boot-time debounce of button
#define BOOT_SERIAL_WAIT_MS            2500   // wait for Serial Monitor attach
