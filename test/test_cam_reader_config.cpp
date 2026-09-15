/**
 * test_cam_reader_config.cpp — env `esp32cam-reader` (reader image on an
 * ESP32-CAM board, camera-less) hardware map, pinned at COMPILE time.
 *
 * The whole point of the profile: the RC522 harness is wired exactly like
 * the station, so this TU includes BOTH maps and asserts they agree, plus
 * that the reader-on-CAM map never picks up the DevKit's LED/buzzer pins
 * (25/26 are camera-bus lines, 33 is the onboard LED) or the shutter.
 */
#include <unity.h>

#include "config/esp32cam_reader.h"

static_assert(PIN_RC522_SS == 13 && PIN_RC522_SCK == 14 && PIN_RC522_MOSI == 15
                  && PIN_RC522_MISO == 2,
              "esp32cam-reader RC522 must use the station SPI pins (13/14/15/2)");
static_assert(PIN_RC522_RST < 0, "RC522 RST stays strapped to 3V3 (GPIO4 is the flash LED)");
static_assert(PIN_STATION_LED == 33, "feedback lives on the onboard red LED (GPIO33)");

#ifdef PIN_LED_MODE
#error "esp32cam-reader must not define PIN_LED_MODE (DevKit LED pins do not exist on the CAM board)"
#endif
#ifdef PIN_LED_EVENT
#error "esp32cam-reader must not define PIN_LED_EVENT"
#endif
#ifdef PIN_BUZZER
#error "esp32cam-reader must not define PIN_BUZZER (no free pin)"
#endif
#ifdef PIN_SHUTTER_BUTTON
#error "esp32cam-reader has no camera/shutter — PIN_SHUTTER_BUTTON must stay station-only"
#endif
#ifdef PWDN_GPIO_NUM
#error "esp32cam-reader must not pull in the camera bus map"
#endif

static void cam_reader_config_compiles_with_station_pins() {
    TEST_ASSERT_EQUAL(13, PIN_RC522_SS);
}

void runCamReaderConfigTests() {
    RUN_TEST(cam_reader_config_compiles_with_station_pins);
}
