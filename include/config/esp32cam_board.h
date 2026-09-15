/**
 * config/esp32cam_board.h — the AI-Thinker ESP32-CAM BOARD pins shared by
 * every image flashed onto that board: the camera station (env `esp32cam`)
 * and the camera-less reader (env `esp32cam-reader`). ONE definition, so
 * the RC522 wiring on a CAM board never depends on which image it runs.
 * / Pines de la PLACA ESP32-CAM compartidos por la estación con cámara y
 * el lector sin cámara: UNA definición, el cableado del RC522 no depende
 * de la imagen flasheada.
 *
 *   RC522:  SDA/SS → 13 | SCK → 14 | MOSI → 15 | MISO → 2 | RST → 3V3
 *           (bench temp: RST strapped to 3V3, soft reset only —
 *           PIN_RC522_RST stays -1 so the firmware never drives a pin
 *           for it. SCK/MOSI/MISO/SS double as the SD-slot pins; the
 *           SD card is intentionally unused — never init SD_MMC/SD.)
 *   GPIO4:  ONBOARD FLASH LED — NEVER drive it (firmware leaves it alone).
 *   PSRAM:  GPIO16/17 RESERVED — never use 16 for RC522 RST.
 *   LEDs:   single red LED on GPIO33 (active-LOW on AI-Thinker). GPIO25/26
 *           are camera VSYNC/SIOD — not LEDs on this board.
 */
#pragma once

#include "config/common.h"

// --- RC522 (same SPI numbers as the reader bench) --------------------------
#define PIN_RC522_SCK   14
#define PIN_RC522_MISO  2
#define PIN_RC522_MOSI  15
#define PIN_RC522_SS    13
// Bench temp: RC522 RST strapped to 3V3 (soft reset only) — GPIO4 is the
// onboard flash LED and must not be driven. The MFRC522 library treats
// 255 (UINT8_MAX, UNUSED_PIN) as "no RST pin"; (uint8_t)-1 is exactly that.
#define PIN_RC522_RST   -1

// --- One LED ----------------------------------------------------------------
// TASK-010 (LED follow-up): polarity is a CONFIG value, not a buried
// assumption. AI-Thinker genuine boards: red LED on GPIO33 is ACTIVE-LOW
// (ON = pin LOW). Some clone boards invert it — on those the LED sits
// SOLID ON with the default and every pattern looks inverted. Flip this
// one define to 0 on such a board; nothing else changes.
#define PIN_STATION_LED              33
#define PIN_STATION_LED_ACTIVE_LOW   1
