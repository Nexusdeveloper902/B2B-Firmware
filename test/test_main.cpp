/**
 * test_main.cpp — the single native-test entry point.
 * test_main.cpp — punto de entrada único de las pruebas nativas.
 *
 * PlatformIO's native environment links every test/*.cpp into ONE
 * program, so exactly one main() lives here; each test file exposes a
 * registrar (run*Tests) that queues its RUN_TEST calls.
 * / El entorno native de PlatformIO enlaza todo test/*.cpp en UN solo
 * programa: un único main() aquí; cada archivo expone un registrador.
 */
#include <unity.h>

void runPayloadTests();
void runResponseTests();
void runModeTests();
void runDebounceTests();
void runConsoleTests();
void runAuthTests();

// Shared by every test (Unity calls these around each RUN_TEST).
void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();

    runPayloadTests();    // JSON request payloads (tap + pairing)
    runResponseTests();   // response parsing: every documented case
    runModeTests();       // mode strategies + feedback mapping
    runDebounceTests();   // card debounce + LED patterns/players
    runConsoleTests();    // TASK-003: serial console (LineBuffer + ModeConsole)
    runAuthTests();       // TASK-007: Authorization header VALUE (Bearer)

    return UNITY_END();
}
