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
// ESP32 hardware SPI (VSPI) pins — explicit so non-default wiring can be
// changed here without touching code. Defaults are the VSPI bus pins.
// / Pines SPI explícitos (bus VSPI) para poder cambiar el cableado aquí.
#define PIN_RC522_SCK   18  // RC522 SCK  (VSPI clock)   — confirm wiring
#define PIN_RC522_MISO  19  // RC522 MISO (VSPI data in) — confirm wiring
#define PIN_RC522_MOSI  23  // RC522 MOSI (VSPI data out)— confirm wiring
#define PIN_RC522_SS    5   // RC522 SDA (SS)            — confirm wiring
#define PIN_RC522_RST   27  // RC522 RST                 — confirm wiring

// When the reader fails init (or dies at runtime: wiring glitch, ESD,
// brown-out), retry PCD_Init on this cadence instead of staying dead
// until a reboot. millis()-based, non-blocking.
// / Cadencia de reintento de PCD_Init si el lector falla (cableado, ESD,
// caída de tensión). Basado en millis(), no bloqueante.
#define RC522_REINIT_INTERVAL_MS  5000

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
