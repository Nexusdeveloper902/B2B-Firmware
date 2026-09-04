#include <unity.h>

#include <cstring>
#include <ArduinoJson.h>

#include <string>

#include "PayloadBuilder.h"

// --- tap payload ------------------------------------------------------------

void test_tap_payload_has_required_fields(void) {
    const std::string json = Presence::buildTapPayload("A1B2C3D4");

    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"credential_uid\""));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "A1B2C3D4"));
}

void test_tap_payload_omits_timestamp_when_absent(void) {
    const std::string json = Presence::buildTapPayload("A1B2C3D4");

    TEST_ASSERT_NULL(strstr(json.c_str(), "client_timestamp"));
}

void test_tap_payload_includes_timestamp_when_given(void) {
    const std::string json =
        Presence::buildTapPayload("A1B2C3D4", "2026-09-05T07:58:00-05:00");

    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"client_timestamp\""));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "2026-09-05T07:58:00-05:00"));
}

void test_tap_payload_is_valid_json(void) {
    const std::string json = Presence::buildTapPayload("DEADBEEF42");

    // Round-trip through ArduinoJson (the same parser the device uses).
    JsonDocument doc;
    TEST_ASSERT_TRUE(deserializeJson(doc, json) == DeserializationError::Ok);
    const char* uid = doc["credential_uid"];
    TEST_ASSERT_EQUAL_STRING("DEADBEEF42", uid);
}

void test_tap_payload_escapes_special_characters(void) {
    // A UID must never break the JSON envelope.
    const std::string json = Presence::buildTapPayload("AB\"CD");

    JsonDocument doc;
    TEST_ASSERT_TRUE(deserializeJson(doc, json) == DeserializationError::Ok);
    const char* uid = doc["credential_uid"];
    TEST_ASSERT_EQUAL_STRING("AB\"CD", uid);
}

// --- pair payload -----------------------------------------------------------

void test_pair_payload_has_credential_uid_only(void) {
    const std::string json = Presence::buildPairPayload("1234ABCD");

    JsonDocument doc;
    TEST_ASSERT_TRUE(deserializeJson(doc, json) == DeserializationError::Ok);
    const char* uid = doc["credential_uid"];
    TEST_ASSERT_EQUAL_STRING("1234ABCD", uid);
    TEST_ASSERT_TRUE(doc["client_timestamp"].isNull());
}

void runPayloadTests() {
    RUN_TEST(test_tap_payload_has_required_fields);
    RUN_TEST(test_tap_payload_omits_timestamp_when_absent);
    RUN_TEST(test_tap_payload_includes_timestamp_when_given);
    RUN_TEST(test_tap_payload_is_valid_json);
    RUN_TEST(test_tap_payload_escapes_special_characters);
    RUN_TEST(test_pair_payload_has_credential_uid_only);
}
