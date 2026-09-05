/**
 * MockSerialNfcReader.h — serial-simulated NFC reader.
 * MockSerialNfcReader.h — lector NFC simulado por Serial.
 *
 * Development without the physical board: main.cpp hands every console
 * line that is NOT the mode password to this reader via pushLine(), and
 * it behaves exactly like a card tap. Since TASK-003 the mock reader no
 * longer owns the Serial stream — the composition root (main.cpp) reads
 * Serial once and dispatches: password → ModeConsole, anything else →
 * virtual tap here. / Desarrollo sin placa: main.cpp entrega aquí cada
 * línea de consola que NO es la contraseña de modo (pushLine) y se
 * comporta como un toque real. Desde TASK-003 este lector ya no es dueño
 * del Serial — la raíz de composición lee Serial y despacha.
 *
 * Build selection: platformio.ini env esp32dev-mock defines
 * PRESENCE_READER_IMPL_MOCK; env esp32dev defines PRESENCE_READER_IMPL_RC522.
 */
#pragma once

#include <string>

#include "NfcReader.h"

namespace Presence {

class MockSerialNfcReader : public NfcReader {
public:
    MockSerialNfcReader() = default;

    bool begin() override {
        // Serial is started and read by main.cpp; nothing to initialize.
        return true;
    }

    /**
     * Queue one console line as a virtual card tap (empty lines are
     * ignored). Takes effect on the next poll().
     * / Encola una línea de consola como toque virtual.
     */
    void pushLine(const std::string& line) {
        if (!line.empty()) {
            pendingUid_ = line;  // newest wins; taps are human-paced
        }
    }

    bool poll(std::string& uidOut) override {
        if (pendingUid_.empty()) {
            return false;
        }
        uidOut = pendingUid_;
        pendingUid_.clear();
        return true;  // exactly one tap per pushed line
    }

    const char* label() const override { return "MOCK (Serial input)"; }

private:
    std::string pendingUid_;
};

}  // namespace Presence
