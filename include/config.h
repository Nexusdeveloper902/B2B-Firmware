/**
 * config.h — hardware map dispatcher: ONE authoritative definition per
 * build target. / Despachador del mapa de hardware: UNA definición
 * autoritativa por objetivo de compilación.
 *
 *   env `esp32cam`         (station, -DCAMERA_STATION)       → config/esp32cam.h
 *   env `esp32cam-reader`  (reader on a CAM board,
 *                           -DREADER_ON_CAM_BOARD)           → config/esp32cam_reader.h
 *   envs `esp32dev`(*)     (reader DevKit)                   → config/esp32dev.h
 *
 * Pin values live ONLY in those files — never here. Wiring tables:
 * docs/CAMERA_STATION.md (station) and docs/HARDWARE_SETUP.md (reader).
 */
#pragma once

#ifdef CAMERA_STATION
#include "config/esp32cam.h"
#elif defined(READER_ON_CAM_BOARD)
#include "config/esp32cam_reader.h"
#else
#include "config/esp32dev.h"
#endif

// Flashed-build identity, printed in the boot banner. BENCH RULE: bump
// on every behavior-changing flash so serial logs self-identify (we
// lost rounds to testing stale builds). Format hce.<n>, counting the
// HCE bench flashes that preceded the scheme.
#define PULSE_FW_BUILD "hce.19"  // TASK-015: per-credential HCE keys — reader relays proof, ENROLL in pairing, no HCE_SECRET
