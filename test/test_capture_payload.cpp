/**
 * test_capture_payload.cpp — TASK-008: the multipart wire bytes.
 * test_capture_payload.cpp — TASK-008: los bytes exactos del multipart.
 *
 * The camera uploads JPEGs as multipart/form-data. The EXACT bytes are
 * pinned here on the host so a device-side regression (wrong boundary,
 * missing CRLF, corrupted JPEG interior, mismatched Content-Type) can
 * never reach a bench session.
 */
#include <unity.h>

#include <cstring>
#include <string>

#include "CapturePayload.h"
#include "PayloadBuilder.h"

using namespace Presence;

// A tiny fake "JPEG" with bytes that stress the format: CRLF pairs,
// NUL bytes, high-bit bytes — all must ride verbatim.
static const uint8_t FAKE_JPEG[] = {
    0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 'J', 'F', 'I', 'F', 0x00,
    0x0D, 0x0A, 0x0D, 0x0A, 0xC3, 0x28, 0xFF, 0xD9,
};

// The boundary is stable and the Content-Type names it exactly.
static void content_type_matches_the_boundary() {
    std::string expected = std::string("multipart/form-data; boundary=") + CapturePayload::boundary();
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), CapturePayload::contentType().c_str());
}
// The image-only body (bottle-first) wraps ONE file field.
static void image_only_body_is_a_wellformed_single_file_multipart() {
    std::string body = CapturePayload::imageOnly(FAKE_JPEG, sizeof(FAKE_JPEG));
    std::string b = CapturePayload::boundary();

    // Prefix: file part header with the image field name.
    std::string expectedHead = "--" + b +
        "\r\nContent-Disposition: form-data; name=\"image\"; filename=\"capture.jpg\"\r\n"
        "Content-Type: image/jpeg\r\n\r\n";
    TEST_ASSERT_EQUAL_STRING(expectedHead.c_str(), body.substr(0, expectedHead.size()).c_str());

    // Suffix: closing boundary.
    std::string expectedTail = "\r\n--" + b + "--\r\n";
    TEST_ASSERT_EQUAL_STRING(expectedTail.c_str(), body.substr(body.size() - expectedTail.size()).c_str());

    // Interior: the JPEG bytes verbatim, exactly once.
    TEST_ASSERT_EQUAL_UINT32((uint32_t) (expectedHead.size() + sizeof(FAKE_JPEG) + expectedTail.size()),
                             (uint32_t) body.size());
    TEST_ASSERT_EQUAL_MEMORY(FAKE_JPEG, body.data() + expectedHead.size(), sizeof(FAKE_JPEG));
}

// Binary safety: CRLF/NUL/high-bit bytes inside the JPEG survive.
static void jpeg_binary_bytes_survive_verbatim() {
    std::string body = CapturePayload::imageOnly(FAKE_JPEG, sizeof(FAKE_JPEG));

    std::string head = "--" + std::string(CapturePayload::boundary()) +
        "\r\nContent-Disposition: form-data; name=\"image\"; filename=\"capture.jpg\"\r\n"
        "Content-Type: image/jpeg\r\n\r\n";
    TEST_ASSERT_EQUAL_MEMORY(FAKE_JPEG, body.data() + head.size(), sizeof(FAKE_JPEG));

    // The double CRLF inside the JPEG (indices 11..14: 0D 0A 0D 0A) must
    // not have been collapsed or expanded.
    TEST_ASSERT_EQUAL_UINT8(0x0D, (uint8_t) body[head.size() + 11]);
    TEST_ASSERT_EQUAL_UINT8(0x0A, (uint8_t) body[head.size() + 12]);
    TEST_ASSERT_EQUAL_UINT8(0x0D, (uint8_t) body[head.size() + 13]);
    TEST_ASSERT_EQUAL_UINT8(0x0A, (uint8_t) body[head.size() + 14]);
}

// The classify body (card-first) carries event_id BEFORE the image.
static void classify_body_carries_event_id_then_image() {
    std::string body = CapturePayload::classifyWithEvent(421, FAKE_JPEG, sizeof(FAKE_JPEG));
    std::string b = CapturePayload::boundary();

    std::string expectedHead = "--" + b +
        "\r\nContent-Disposition: form-data; name=\"event_id\"\r\n\r\n421\r\n"
        "--" + b +
        "\r\nContent-Disposition: form-data; name=\"image\"; filename=\"capture.jpg\"\r\n"
        "Content-Type: image/jpeg\r\n\r\n";
    TEST_ASSERT_EQUAL_STRING(expectedHead.c_str(), body.substr(0, expectedHead.size()).c_str());
    TEST_ASSERT_EQUAL_MEMORY(FAKE_JPEG, body.data() + expectedHead.size(), sizeof(FAKE_JPEG));
}

// An empty JPEG (a capture that failed to fill its buffer) still builds
// a structurally valid body — the backend's validation is the judge.
static void empty_image_still_builds_valid_structure() {
    std::string body = CapturePayload::imageOnly(nullptr, 0);
    std::string b = CapturePayload::boundary();
    std::string head = "--" + b +
        "\r\nContent-Disposition: form-data; name=\"image\"; filename=\"capture.jpg\"\r\n"
        "Content-Type: image/jpeg\r\n\r\n";
    std::string tail = "\r\n--" + b + "--\r\n";
    TEST_ASSERT_EQUAL_UINT32((uint32_t) (head.size() + tail.size()), (uint32_t) body.size());
}

// The associate JSON body matches the B2B-Core contract.
static void associate_payload_matches_the_backend_contract() {
    TEST_ASSERT_EQUAL_STRING("{\"credential_uid\":\"ABC123\"}",
                             buildAssociatePayload("ABC123").c_str());
}

void runCapturePayloadTests() {
    RUN_TEST(content_type_matches_the_boundary);
    RUN_TEST(image_only_body_is_a_wellformed_single_file_multipart);
    RUN_TEST(jpeg_binary_bytes_survive_verbatim);
    RUN_TEST(classify_body_carries_event_id_then_image);
    RUN_TEST(empty_image_still_builds_valid_structure);
    RUN_TEST(associate_payload_matches_the_backend_contract);
}
