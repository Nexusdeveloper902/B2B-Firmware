/**
 * test_auth.cpp — TASK-007: the Authorization header VALUE contract.
 * test_auth.cpp — TASK-007: el contrato del VALOR de Authorization.
 *
 * The "Bearer " prefix lives in PresenceCore (host-testable) precisely
 * so these tests can pin it. The bug they guard against is real
 * history: the ESP32 Arduino HTTPClient's setAuthorization(key) sends
 * "Authorization: Basic <key>" (its default authorization TYPE), Laravel
 * only reads the Bearer scheme, and every real-hardware call answered
 * 401 with a perfectly valid key — while curl verification (which sends
 * the header verbatim) kept passing.
 * / El prefijo "Bearer " vive en PresenceCore (testeable en el host)
 * para que estas pruebas lo fijen. El bug que evitan es historia real:
 * setAuthorization(key) envia "Authorization: Basic <clave>" (tipo por
 * defecto de HTTPClient), Laravel solo lee el esquema Bearer, y todo el
 * hardware real recibia 401 con una clave perfectamente valida.
 *
 * The header NAME ("Authorization") lives in EspApiClient (Arduino-only,
 * not host-compilable) and is covered by docs/API_INTEGRATION.md and the
 * owner's bench checklist instead.
 * / El NOMBRE de la cabecera vive en EspApiClient (solo Arduino, no
 * compilable en el host); lo cubren docs/API_INTEGRATION.md y la lista
 * de verificacion del propietario.
 */
#include <unity.h>

#include <string>

#include "CoreTypes.h"

using namespace Presence;

// The exact scheme prefix, byte for byte — never "Basic", never lowercase.
// / El prefijo de esquema exacto, byte a byte — nunca "Basic", nunca minusculas.
static void bearer_value_has_the_exact_scheme_prefix() {
    TEST_ASSERT_EQUAL_STRING("Bearer abc123",
                             bearerAuthorizationValue("abc123").c_str());
}

// A full 32-character reader key passes through unchanged (no trimming,
// no case folding — the backend compares it exactly).
// / Una clave de lector completa de 32 caracteres pasa sin cambios (sin
// recortes ni cambio de mayusculas — el backend la compara exacta).
static void bearer_value_passes_a_full_32_char_key_unchanged() {
    const std::string key = "12345678901234567890123456789012";
    const std::string value = bearerAuthorizationValue(key);

    TEST_ASSERT_EQUAL_STRING("Bearer 12345678901234567890123456789012",
                             value.c_str());
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(7 + 32), (uint32_t)value.size());
}

// An empty key still yields the bare scheme (the backend answers 401
// "missing bearer token" — the same remediation path as a bad key).
// / Una clave vacia produce el esquema pelado (el backend responde 401
// "missing bearer token" — la misma ruta de arreglo que una clave mala).
static void bearer_value_with_empty_key_is_the_bare_scheme() {
    TEST_ASSERT_EQUAL_STRING("Bearer ", bearerAuthorizationValue("").c_str());
}

void runAuthTests() {
    RUN_TEST(bearer_value_has_the_exact_scheme_prefix);
    RUN_TEST(bearer_value_passes_a_full_32_char_key_unchanged);
    RUN_TEST(bearer_value_with_empty_key_is_the_bare_scheme);
}
