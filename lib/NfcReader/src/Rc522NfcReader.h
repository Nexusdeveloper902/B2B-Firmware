/**
 * Rc522NfcReader.h — concrete NFC reader: MFRC522 over SPI.
 * Rc522NfcReader.h — lector NFC concreto: MFRC522 por SPI.
 *
 * Uses the well-established miguelbalboa/rfid Arduino library. Pins come
 * from include/config.h (PIN_RC522_SS / PIN_RC522_RST + hardware VSPI).
 * / Usa la librería miguelbalboa/rfid. Los pines vienen de config.h.
 */
#pragma once

#include <MFRC522.h>
#include <SPI.h>

#include "NfcReader.h"

namespace Presence {

class Rc522NfcReader : public NfcReader {
public:
    Rc522NfcReader(uint8_t ssPin, uint8_t rstPin)
        : mfrc522_(ssPin, rstPin) {}

    bool begin() override {
        SPI.begin();
        mfrc522_.PCD_Init();
        // A failed init often reports version 0x00 / 0xFF — log it, but the
        // reader stays installed so the device remains responsive.
        // Valid RC522 versions: 0x91 (1.0), 0x92 (2.0); 0x90 = clone-ish.
        uint8_t version = mfrc522_.PCD_ReadRegister(MFRC522::PCD_Register::VersionReg);
        return version != 0x00 && version != 0xFF;
    }

    bool poll(std::string& uidOut) override {
        if (!mfrc522_.PICC_IsNewCardPresent()) {
            return false;
        }
        if (!mfrc522_.PICC_ReadCardSerial()) {
            return false;
        }

        uidOut.clear();
        uidOut.reserve(mfrc522_.uid.size * 2);
        static const char* hex = "0123456789ABCDEF";
        for (byte i = 0; i < mfrc522_.uid.size; ++i) {
            uidOut += hex[mfrc522_.uid.uidByte[i] >> 4];
            uidOut += hex[mfrc522_.uid.uidByte[i] & 0x0F];
        }

        // Release the card and stop any crypto session so the next tap works.
        mfrc522_.PICC_HaltA();
        mfrc522_.PCD_StopCrypto1();
        return true;
    }

    const char* label() const override { return "RC522 (SPI)"; }

private:
    MFRC522 mfrc522_;
};

}  // namespace Presence
