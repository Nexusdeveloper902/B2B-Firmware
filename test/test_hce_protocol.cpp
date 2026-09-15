#include <unity.h>

#include <cstring>
#include <string>

#include "HceProtocol.h"

namespace {

// --- helpers ---------------------------------------------------------------

// Builds a well-formed CHALLENGE response for cred + nonce under key.
void makeChallengeResponse(const std::string& cred, const uint8_t nonce[8],
                           const uint8_t* key, size_t keyLen,
                           uint8_t* out, size_t* outLen) {
    uint8_t msg[64];
    memcpy(msg, cred.data(), cred.size());
    memcpy(msg + cred.size(), nonce, 8);
    uint8_t mac[32];
    Presence::Hce::hmacSha256(key, keyLen, msg, cred.size() + 8, mac);
    out[0] = static_cast<uint8_t>(cred.size());
    memcpy(out + 1, cred.data(), cred.size());
    memcpy(out + 1 + cred.size(), mac, 32);
    out[1 + cred.size() + 32] = 0x90;
    out[1 + cred.size() + 32 + 1] = 0x00;
    *outLen = 1 + cred.size() + 32 + 2;
}

}  // namespace

// --- SELECT builder: byte-exact vs the prototype ----------------------------

void test_hce_select_builder_is_byte_exact(void) {
    uint8_t out[16];
    const size_t n = Presence::Hce::buildSelectAid(out, sizeof(out));

    const uint8_t want[] = {0x00, 0xA4, 0x04, 0x00, 0x07,
                            0xF0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    TEST_ASSERT_EQUAL_size_t(sizeof(want), n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want, out, sizeof(want));
}

void test_hce_select_builder_refuses_short_buffer(void) {
    uint8_t out[11];  // one byte short of the 12-byte SELECT
    TEST_ASSERT_EQUAL_size_t(0, Presence::Hce::buildSelectAid(out, sizeof(out)));
}

// --- CHALLENGE builder: byte-exact vs the prototype --------------------------

void test_hce_challenge_builder_is_byte_exact(void) {
    const uint8_t nonce[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    uint8_t out[16];
    const size_t n = Presence::Hce::buildChallenge(nonce, out, sizeof(out));

    const uint8_t want[] = {0x80, 0x10, 0x00, 0x00, 0x08,
                            0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    TEST_ASSERT_EQUAL_size_t(sizeof(want), n);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want, out, sizeof(want));
}

// --- SELECT parser -----------------------------------------------------------

void test_hce_select_parser_accepts_hce_ok(void) {
    const uint8_t resp[] = {'H', 'C', 'E', '-', 'O', 'K', 0x90, 0x00};
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Presence::Hce::SelectOutcome::Selected),
                          static_cast<int>(Presence::Hce::parseSelectResponse(resp, sizeof(resp))));
}

void test_hce_select_parser_reports_unknown_aid(void) {
    const uint8_t resp[] = {0x6A, 0x82};
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Presence::Hce::SelectOutcome::UnknownAid),
                          static_cast<int>(Presence::Hce::parseSelectResponse(resp, sizeof(resp))));
}

void test_hce_select_parser_rejects_malformed(void) {
    // Truncated, wrong payload with 9000, 9000 alone, empty.
    const uint8_t truncated[] = {0x90};
    const uint8_t wrongPayload[] = {'N', 'O', 'P', 'E', '!', '!', 0x90, 0x00};
    const uint8_t bare9000[] = {0x90, 0x00};
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Presence::Hce::SelectOutcome::Malformed),
                          static_cast<int>(Presence::Hce::parseSelectResponse(truncated, sizeof(truncated))));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Presence::Hce::SelectOutcome::Malformed),
                          static_cast<int>(Presence::Hce::parseSelectResponse(wrongPayload, sizeof(wrongPayload))));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Presence::Hce::SelectOutcome::Malformed),
                          static_cast<int>(Presence::Hce::parseSelectResponse(bare9000, sizeof(bare9000))));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Presence::Hce::SelectOutcome::Malformed),
                          static_cast<int>(Presence::Hce::parseSelectResponse(nullptr, 0)));
}

// --- CHALLENGE parser ----------------------------------------------------------

void test_hce_challenge_parser_round_trip(void) {
    static const uint8_t kKey[] = "dev-only-prototype-secret-001";
    const uint8_t nonce[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44};
    uint8_t resp[64];
    size_t respLen = 0;
    makeChallengeResponse("TEST-ANDROID-001", nonce, kKey, sizeof(kKey) - 1, resp, &respLen);
    TEST_ASSERT_EQUAL_size_t(51, respLen);  // 1 + 16 + 32 + 2, per the spec

    Presence::Hce::ChallengeResponse parsed;
    TEST_ASSERT_TRUE(Presence::Hce::parseChallengeResponse(resp, respLen, parsed));
    TEST_ASSERT_TRUE(parsed.ok);
    TEST_ASSERT_EQUAL_STRING("TEST-ANDROID-001", parsed.credId.c_str());
    TEST_ASSERT_TRUE(Presence::Hce::verifyChallengeResponse(parsed, nonce, kKey, sizeof(kKey) - 1));
}

void test_hce_challenge_parser_rejects_malformed(void) {
    Presence::Hce::ChallengeResponse out;
    // Bad length prefix (claims 16, frame holds 4).
    const uint8_t badPrefix[] = {0x10, 'A', 'B', 'C', 'D', 0x90, 0x00};
    TEST_ASSERT_FALSE(Presence::Hce::parseChallengeResponse(badPrefix, sizeof(badPrefix), out));
    // Zero-length credential.
    const uint8_t zeroLen[1 + 32 + 2] = {0x00};
    TEST_ASSERT_FALSE(Presence::Hce::parseChallengeResponse(zeroLen, sizeof(zeroLen), out));
    // Truncated MAC.
    const uint8_t truncated[] = {0x04, 'A', 'B', 'C', 'D', 0x01, 0x02, 0x90, 0x00};
    TEST_ASSERT_FALSE(Presence::Hce::parseChallengeResponse(truncated, sizeof(truncated), out));
    // Wrong status word.
    uint8_t wrongSw[64];
    size_t wrongSwLen = 0;
    static const uint8_t kKey[] = "k";
    const uint8_t nonce[8] = {0};
    makeChallengeResponse("ABCD", nonce, kKey, sizeof(kKey) - 1, wrongSw, &wrongSwLen);
    wrongSw[wrongSwLen - 2] = 0x6F;
    wrongSw[wrongSwLen - 1] = 0x00;
    TEST_ASSERT_FALSE(Presence::Hce::parseChallengeResponse(wrongSw, wrongSwLen, out));
}

// --- HMAC-SHA256: known-answer + tamper ---------------------------------------

void test_hce_hmac_matches_rfc4231_vector(void) {
    // RFC 4231 test case 1: key = 20 x 0x0b, data = "Hi There".
    uint8_t key[20];
    memset(key, 0x0b, sizeof(key));
    static const uint8_t data[] = "Hi There";
    uint8_t mac[32];
    Presence::Hce::hmacSha256(key, sizeof(key), data, sizeof(data) - 1, mac);
    static const uint8_t want[32] = {
        0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf,
        0xce, 0xaf, 0x0b, 0xf1, 0x2b, 0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83,
        0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(want, mac, sizeof(want));
}

void test_hce_verify_rejects_wrong_key_nonce_or_mac(void) {
    static const uint8_t kKey[] = "dev-only-prototype-secret-001";
    static const uint8_t kOther[] = "another-secret-00000000000002";
    const uint8_t nonce[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    const uint8_t otherNonce[8] = {8, 7, 6, 5, 4, 3, 2, 1};
    uint8_t resp[64];
    size_t respLen = 0;
    makeChallengeResponse("TEST-ANDROID-001", nonce, kKey, sizeof(kKey) - 1, resp, &respLen);

    Presence::Hce::ChallengeResponse parsed;
    TEST_ASSERT_TRUE(Presence::Hce::parseChallengeResponse(resp, respLen, parsed));
    TEST_ASSERT_TRUE(Presence::Hce::verifyChallengeResponse(parsed, nonce, kKey, sizeof(kKey) - 1));
    // Wrong key, wrong nonce, tampered MAC all fail closed.
    TEST_ASSERT_FALSE(Presence::Hce::verifyChallengeResponse(parsed, nonce, kOther, sizeof(kOther) - 1));
    TEST_ASSERT_FALSE(Presence::Hce::verifyChallengeResponse(parsed, otherNonce, kKey, sizeof(kKey) - 1));
    parsed.mac[0] ^= 0x01;
    TEST_ASSERT_FALSE(Presence::Hce::verifyChallengeResponse(parsed, nonce, kKey, sizeof(kKey) - 1));
}

// --- UID-independence: the RF UID is not an input -------------------------------
// These parsers do not even TAKE an RF UID — the same APDU bytes parse to
// the same application-level credential no matter which random UID the
// phone presented on the RF layer. Pinned here so a future refactor can
// never smuggle the UID into the identity path.

void test_hce_identity_ignores_the_rf_uid(void) {
    static const uint8_t kKey[] = "dev-only-prototype-secret-001";
    const uint8_t nonce[8] = {9, 9, 9, 9, 9, 9, 9, 9};
    uint8_t resp[64];
    size_t respLen = 0;
    makeChallengeResponse("TEST-ANDROID-001", nonce, kKey, sizeof(kKey) - 1, resp, &respLen);

    // Two taps, two different randomized RF UIDs — the APDU bytes are what
    // they are; the UID values below are never passed to any parser.
    const uint8_t rfUidTap1[] = {0x08, 0x11, 0x22, 0x33};
    const uint8_t rfUidTap2[] = {0x08, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    (void)rfUidTap1;
    (void)rfUidTap2;

    Presence::Hce::ChallengeResponse first, second;
    TEST_ASSERT_TRUE(Presence::Hce::parseChallengeResponse(resp, respLen, first));
    TEST_ASSERT_TRUE(Presence::Hce::parseChallengeResponse(resp, respLen, second));
    TEST_ASSERT_EQUAL_STRING(first.credId.c_str(), second.credId.c_str());
    TEST_ASSERT_EQUAL_STRING("TEST-ANDROID-001", first.credId.c_str());
    TEST_ASSERT_TRUE(Presence::Hce::verifyChallengeResponse(first, nonce, kKey, sizeof(kKey) - 1));
}

void runHceProtocolTests() {
    RUN_TEST(test_hce_select_builder_is_byte_exact);
    RUN_TEST(test_hce_select_builder_refuses_short_buffer);
    RUN_TEST(test_hce_challenge_builder_is_byte_exact);
    RUN_TEST(test_hce_select_parser_accepts_hce_ok);
    RUN_TEST(test_hce_select_parser_reports_unknown_aid);
    RUN_TEST(test_hce_select_parser_rejects_malformed);
    RUN_TEST(test_hce_challenge_parser_round_trip);
    RUN_TEST(test_hce_challenge_parser_rejects_malformed);
    RUN_TEST(test_hce_hmac_matches_rfc4231_vector);
    RUN_TEST(test_hce_verify_rejects_wrong_key_nonce_or_mac);
    RUN_TEST(test_hce_identity_ignores_the_rf_uid);
}
