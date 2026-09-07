/**
 * config/esp32dev.h — READER board (envs `esp32dev`, `esp32dev-mock`).
 * config/esp32dev.h — placa LECTORA.
 *
 * Generic ESP32 DevKit with the RC522 over SPI (VSPI) and free GPIOs
 * for LEDs/buzzer. The RC522 pin numbers match the bench wiring shared
 * with the station (see docs/HARDWARE_SETUP.md); the NOT-responding
 * serial line always prints the expected pins — trust it over this file.
 */
#pragma once

#include "config/common.h"

// --- NFC reader: RC522 over SPI -------------------------------------------
#define PIN_RC522_SCK   14  // RC522 SCK  (VSPI clock)   — confirm wiring
#define PIN_RC522_MISO  2   // RC522 MISO (VSPI data in) — confirm wiring
#define PIN_RC522_MOSI  15  // RC522 MOSI (VSPI data out)— confirm wiring
#define PIN_RC522_SS    13  // RC522 SDA (SS)            — confirm wiring
#define PIN_RC522_RST   4   // RC522 RST — bench-verified 2026-09-07

// --- Feedback hardware ----------------------------------------------------
#define PIN_LED_MODE    25  // mode indicator LED — confirm wiring
#define PIN_LED_EVENT   26  // event feedback LED — confirm wiring
#define PIN_BUZZER      33  // optional passive buzzer; set to -1 if absent
