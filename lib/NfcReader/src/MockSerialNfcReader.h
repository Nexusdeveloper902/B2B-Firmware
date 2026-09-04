/**
 * MockSerialNfcReader.h — serial-simulated NFC reader.
 * MockSerialNfcReader.h — lector NFC simulado por Serial.
 *
 * Development without the physical board: type a card UID (any text, e.g.
 * "M9TN530AIT7N" or "A1B2C3D4") + Enter in the Serial Monitor and it
 * behaves exactly like a card tap. This is how mode-switching and
 * API-calling logic get exercised with no hardware present (the protocol
 * builds and tests against this reader first).
 * / Desarrollo sin placa física: escribe un UID de tarjeta (cualquier
 * texto) + Enter en el Monitor Serial y se comporta como un toque real.
 *
 * Build selection: platformio.ini env esp32dev-mock defines
 * PRESENCE_READER_IMPL_MOCK; env esp32dev defines PRESENCE_READER_IMPL_RC522.
 */
#pragma once

#include <Arduino.h>

#include "NfcReader.h"

namespace Presence {

class MockSerialNfcReader : public NfcReader {
public:
    explicit MockSerialNfcReader(HardwareSerial& serial = Serial)
        : serial_(serial) {}

    bool begin() override {
        // Serial is started by main.cpp; nothing to initialize here.
        return true;
    }

    bool poll(std::string& uidOut) override {
        while (serial_.available() > 0) {
            char c = static_cast<char>(serial_.read());

            if (c == '\n' || c == '\r') {
                if (!line_.empty()) {
                    // A completed line = one virtual card tap.
                    uidOut = trim(line_);
                    line_.clear();
                    return !uidOut.empty();
                }
                continue;
            }
            line_ += c;
            // Guard against absurdly long input lines.
            if (line_.size() > 128) {
                line_.clear();
            }
        }
        return false;
    }

    const char* label() const override { return "MOCK (Serial input)"; }

private:
    static std::string trim(const std::string& s) {
        size_t begin = s.find_first_not_of(" \t");
        if (begin == std::string::npos) {
            return std::string();
        }
        size_t end = s.find_last_not_of(" \t");
        return s.substr(begin, end - begin + 1);
    }

    HardwareSerial& serial_;
    std::string line_;
};

}  // namespace Presence
