/**
 * test_station_config.cpp — the ESP32-CAM station hardware map, pinned.
 *
 * These are the bench-verified numbers (DIAG-CAM 2026-09-07: VersionReg
 * 0x92 stable + raw SPI agree). If anyone moves a pin, this fails at
 * COMPILE time via static_assert — before any flash. Also covers the two
 * station FeedbackKinds and their patterns (host-testable, Arduino-free).
 */
#include <unity.h>

#include "config/esp32cam.h"
#include "FeedbackPatterns.h"

// --- authoritative station RC522 wiring (do NOT change: validated) ---------
static_assert(PIN_RC522_SS == 13, "station SS must be GPIO13");
static_assert(PIN_RC522_SCK == 14, "station SCK must be GPIO14");
static_assert(PIN_RC522_MOSI == 15, "station MOSI must be GPIO15");
static_assert(PIN_RC522_MISO == 2, "station MISO must be GPIO2");
static_assert(PIN_RC522_RST == 4, "station RST must be GPIO4 (16/17 are PSRAM)");

// --- safety invariants ------------------------------------------------------
static_assert(PIN_CAM_BUZZER < 0, "buzzer must stay absent while RST is on GPIO4");
static_assert(PIN_SHUTTER_BUTTON == 12, "shutter must be GPIO12 (to GND, never 3V3)");

// --- camera bus spot-checks (AI-Thinker map must survive refactors) ---------
static_assert(Y2_GPIO_NUM == 5, "camera Y2");
static_assert(XCLK_GPIO_NUM == 0, "camera XCLK");
static_assert(SIOD_GPIO_NUM == 26, "camera SIOD");
static_assert(VSYNC_GPIO_NUM == 25, "camera VSYNC");

static void station_capture_success_has_solid_pattern() {
    auto pattern = Presence::eventLedPattern(Presence::FeedbackKind::CaptureSuccess);
    TEST_ASSERT_FALSE(pattern.empty());
    TEST_ASSERT_EQUAL_size_t(1, pattern.size());
    TEST_ASSERT_TRUE(pattern[0].on);
}

static void station_degraded_pattern_is_distinct() {
    auto degraded = Presence::modeLedPattern(Presence::FeedbackKind::StationDegraded);
    auto op = Presence::modeLedPattern(Presence::FeedbackKind::IdleOperation);
    auto pair = Presence::modeLedPattern(Presence::FeedbackKind::IdlePairing);
    auto boot = Presence::modeLedPattern(Presence::FeedbackKind::BootConnecting);
    TEST_ASSERT_FALSE(degraded.empty());
    TEST_ASSERT_TRUE(degraded != op);
    TEST_ASSERT_TRUE(degraded != pair);
    TEST_ASSERT_TRUE(degraded != boot);
}

void runStationConfigTests() {
    RUN_TEST(station_capture_success_has_solid_pattern);
    RUN_TEST(station_degraded_pattern_is_distinct);
}
