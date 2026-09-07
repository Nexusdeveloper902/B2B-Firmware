/**
 * config/esp32cam.h — STATION board (env `esp32cam`): AI-Thinker ESP32-CAM
 * (OV3660) with camera + RC522 aboard. THE authoritative CAM hardware map.
 *
 *   RC522:  SDA/SS → 13 | SCK → 14 | MOSI → 15 | MISO → 2 | RST → 4
 *           (bench-verified 2026-09-07: DIAG-CAM VersionReg 0x92 stable
 *           + raw SPI agree. Those five double as the SD-slot pins; the
 *           SD card is intentionally unused — never init SD_MMC/SD.)
 *   PSRAM:  GPIO16/17 RESERVED — never use 16 for RC522 RST.
 *   Buzzer: ABSENT (-1). GPIO4 is RC522 RST; a buzzer idling LOW would
 *           hold the active-LOW RC522 reset forever. Re-enable ONLY on a
 *           genuinely free pin.
 *   LEDs:   single red LED on GPIO33 (active-LOW on AI-Thinker). GPIO25/26
 *           are camera VSYNC/SIOD — not LEDs on this board.
 *   Button: shutter between GPIO12 and GND (internal pull-up, active-LOW).
 *           NEVER tie GPIO12 HIGH — MTDI strapping (flash voltage).
 */
#pragma once

#include "config/common.h"

// --- Station RC522 (same SPI numbers as the reader bench) ------------------
#define PIN_RC522_SCK   14
#define PIN_RC522_MISO  2
#define PIN_RC522_MOSI  15
#define PIN_RC522_SS    13
#define PIN_RC522_RST   4

// --- Station feedback: one LED, no buzzer ----------------------------------
#define PIN_STATION_LED   33    // red LED, active-LOW — confirm polarity
#define PIN_CAM_BUZZER    -1    // keep -1 while RC522 RST is on GPIO4

// --- Shutter button ---------------------------------------------------------
#define PIN_SHUTTER_BUTTON  12   // active-LOW to GND — confirm wiring
#define SHUTTER_DEBOUNCE_MS 50   // contact-settle window per press

// --- Camera recovery: init is heavy (sensor + PSRAM frame buffers), so a
// failed camera retries on a slower cadence than the RC522. millis()-based,
// non-blocking; the station stays alive meanwhile.
#define CAMERA_REINIT_INTERVAL_MS 30000

// --- Camera bus pin map (AI-Thinker, from the verified reference) ----------
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
