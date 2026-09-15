/**
 * test_pulse_endpoint.cpp — TASK-013: the discovered-endpoint contract.
 * test_pulse_endpoint.cpp — TASK-013: el contrato del endpoint descubierto.
 *
 * The SELECTION policy (which DNS-SD answer the device dials) lives in
 * PresenceCore precisely so these tests pin it: a wrong pick here sends
 * every tap at a dead address until the next rediscovery, and the failure
 * looks exactly like a Wi-Fi outage on the bench. The Arduino-only query
 * (ESPmDNS mapping) is covered by the bench checklist instead.
 * / La política de SELECCIÓN vive en PresenceCore para que estas pruebas
 * la fijen: una mala elección manda cada toque a una dirección muerta y
 * el fallo parece una caída de Wi-Fi. La consulta (solo Arduino) la cubre
 * la lista de verificación.
 */
#include <unity.h>

#include <string>

#include "PulseEndpoint.h"

using namespace Presence;

static PulseCandidate candidate(const char* ip, uint16_t port,
                                const char* protocol = "") {
    PulseCandidate c;
    c.ip = ip;
    c.port = port;
    c.protocolTxt = protocol;
    return c;
}

// A single healthy answer dials host:port over plain HTTP.
// / Una respuesta sana marca host:puerto por HTTP plano.
static void endpoint_builds_the_http_base_url() {
    PulseCandidate c = candidate("192.168.1.50", 8000, "1");
    PulseEndpoint out;
    TEST_ASSERT_TRUE(pickPulseEndpoint(&c, 1, out));
    TEST_ASSERT_TRUE(out.valid);
    TEST_ASSERT_EQUAL_STRING("192.168.1.50", out.host.c_str());
    TEST_ASSERT_EQUAL_UINT16(8000, out.port);
    TEST_ASSERT_EQUAL_STRING("http://192.168.1.50:8000",
                             out.baseUrl().c_str());
    TEST_ASSERT_EQUAL_STRING("http://192.168.1.50:8000/api/v1/events/tap",
                             out.urlFor("/api/v1/events/tap").c_str());
}

// The first USABLE answer wins: blanks, 0.0.0.0 and zero ports are skipped.
// / Gana la primera respuesta ÚTIL: se saltan vacías, 0.0.0.0 y puerto cero.
static void selection_skips_unusable_answers() {
    PulseCandidate cs[4];
    cs[0] = candidate("", 8000, "1");
    cs[1] = candidate("0.0.0.0", 8000, "1");
    cs[2] = candidate("192.168.1.9", 0, "1");
    cs[3] = candidate("192.168.43.91", 8000, "1");
    PulseEndpoint out;
    TEST_ASSERT_TRUE(pickPulseEndpoint(cs, 4, out));
    TEST_ASSERT_EQUAL_STRING("192.168.43.91", out.host.c_str());
    TEST_ASSERT_EQUAL_UINT16(8000, out.port);
}

// Nothing usable (or nothing at all) yields an invalid endpoint — the
// caller must keep the compiled fallback, never a blank URL.
// / Sin nada útil queda un endpoint inválido — se conserva el valor
// compilado, nunca una URL vacía.
static void selection_with_no_usable_answer_is_invalid() {
    PulseCandidate cs[2];
    cs[0] = candidate("0.0.0.0", 8000, "1");
    cs[1] = candidate("192.168.1.9", 0, "1");
    PulseEndpoint out;
    TEST_ASSERT_FALSE(pickPulseEndpoint(cs, 2, out));
    TEST_ASSERT_FALSE(out.valid);

    TEST_ASSERT_FALSE(pickPulseEndpoint(0, 0, out));
    TEST_ASSERT_FALSE(out.valid);
}

// protocol TXT gate: absent stays accepted (minimal advertisements keep
// working), "1" is accepted, anything else is a future wire — rejected.
// / Puerta protocol: ausente se acepta, "1" se acepta, otro valor es un
// protocolo futuro — se rechaza.
static void protocol_txt_gates_future_wires() {
    TEST_ASSERT_TRUE(pulseProtocolCompatible(""));
    TEST_ASSERT_TRUE(pulseProtocolCompatible("1"));
    TEST_ASSERT_FALSE(pulseProtocolCompatible("2"));

    PulseCandidate cs[2];
    cs[0] = candidate("192.168.1.50", 8000, "2");
    cs[1] = candidate("192.168.43.91", 8000, "");
    PulseEndpoint out;
    TEST_ASSERT_TRUE(pickPulseEndpoint(cs, 2, out));
    TEST_ASSERT_EQUAL_STRING("192.168.43.91", out.host.c_str());
}

// The compiled API_BASE_URL fallback parses back into an endpoint.
// / El valor compilado API_BASE_URL se reconvierte en endpoint.
static void fallback_base_url_parses() {
    PulseEndpoint out =
        PulseEndpoint::fromBaseUrl("http://192.168.1.11:8000");
    TEST_ASSERT_TRUE(out.valid);
    TEST_ASSERT_EQUAL_STRING("192.168.1.11", out.host.c_str());
    TEST_ASSERT_EQUAL_UINT16(8000, out.port);
    TEST_ASSERT_EQUAL_STRING("http://192.168.1.11:8000",
                             out.baseUrl().c_str());
}

// Garbage fallbacks stay invalid (missing host, bad port) — and a bare
// host without a port means plain port 80.
// / Los valores basura quedan inválidos; un host pelado es puerto 80.
static void fallback_base_url_rejects_garbage() {
    TEST_ASSERT_FALSE(PulseEndpoint::fromBaseUrl("http://:8000").valid);
    TEST_ASSERT_FALSE(PulseEndpoint::fromBaseUrl("http://host:abc").valid);
    TEST_ASSERT_FALSE(PulseEndpoint::fromBaseUrl("http://").valid);

    PulseEndpoint bare = PulseEndpoint::fromBaseUrl("http://pulse-backend");
    TEST_ASSERT_TRUE(bare.valid);
    TEST_ASSERT_EQUAL_UINT16(80, bare.port);
}

void runPulseEndpointTests() {
    RUN_TEST(endpoint_builds_the_http_base_url);
    RUN_TEST(selection_skips_unusable_answers);
    RUN_TEST(selection_with_no_usable_answer_is_invalid);
    RUN_TEST(protocol_txt_gates_future_wires);
    RUN_TEST(fallback_base_url_parses);
    RUN_TEST(fallback_base_url_rejects_garbage);
}
