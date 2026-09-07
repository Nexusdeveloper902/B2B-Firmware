/**
 * config.h — hardware map dispatcher: ONE authoritative definition per
 * build target. / Despachador del mapa de hardware: UNA definición
 * autoritativa por objetivo de compilación.
 *
 *   env `esp32cam`      (station, -DCAMERA_STATION) → config/esp32cam.h
 *   envs `esp32dev`(*)  (reader DevKit)             → config/esp32dev.h
 *
 * Pin values live ONLY in those files — never here. Wiring tables:
 * docs/CAMERA_STATION.md (station) and docs/HARDWARE_SETUP.md (reader).
 */
#pragma once

#ifdef CAMERA_STATION
#include "config/esp32cam.h"
#else
#include "config/esp32dev.h"
#endif
