/**
 * config/esp32cam_reader.h — CAMERA-LESS READER on an ESP32-CAM board
 * (env `esp32cam-reader`). / LECTOR SIN CÁMARA sobre una placa ESP32-CAM.
 *
 * Physically an AI-Thinker ESP32-CAM, logically the plain reader: it runs
 * src/main.cpp (tap/pair pipeline, mode console, HCE) with NO camera
 * code, capture trigger or shutter — the OV sensor need not even be
 * connected. The RC522 sits on the station's pins (esp32cam_board.h), so
 * the same RC522 harness moves between a station and this reader.
 * / Físicamente un ESP32-CAM, lógicamente el lector: src/main.cpp sin
 * código de cámara; el RC522 usa los mismos pines de la estación.
 *
 * Feedback: the CAM board breaks out no free GPIO for the reader's
 * MODE/EVENT LEDs + buzzer (esp32dev.h uses 25/26/33, which are camera
 * bus / the onboard LED here), so both channels share the onboard red
 * LED on GPIO33 by precedence (StationLed) — same blink grammar as
 * docs/HARDWARE_SETUP.md, no buzzer. GPIO4 (flash LED) and GPIO12
 * (MTDI strap) stay untouched.
 */
#pragma once

#include "config/esp32cam_board.h"
