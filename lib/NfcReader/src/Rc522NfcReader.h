/**
 * Rc522NfcReader.h — concrete NFC reader: MFRC522 over SPI.
 * Rc522NfcReader.h — lector NFC concreto: MFRC522 por SPI.
 *
 * Uses the well-established miguelbalboa/rfid Arduino library. Pins come
 * from include/config.h (PIN_RC522_SS / PIN_RC522_RST / PIN_RC522_SCK /
 * PIN_RC522_MISO / PIN_RC522_MOSI + hardware VSPI defaults).
 * / Usa la librería miguelbalboa/rfid. Los pines vienen de config.h.
 *
 * Robustness beyond init (TASK-002): the reader tracks its own health.
 * A failed init (wiring, power) or a reader that dies at runtime is
 * retried on the RC522_REINIT_INTERVAL_MS cadence — non-blocking, so the
 * device self-heals without a reboot. Diagnostic events (init ok/failed,
 * recovery) go to the injected Print stream (Serial in production) or
 * nowhere if none is injected.
 * / Robustez (TASK-002): el lector vigila su propia salud. Un init
 * fallido (cableado, alimentación) o un lector que muere en ejecución se
 * reintenta cada RC522_REINIT_INTERVAL_MS — no bloqueante, el equipo se
 * recupera sin reiniciar. Los diagnósticos van al Print inyectado.
 */
#pragma once

#include <Arduino.h>

#include <MFRC522.h>
#include <SPI.h>

#include "NfcReader.h"
#include "config.h"

namespace Presence {

class Rc522NfcReader : public NfcReader {
public:
    explicit Rc522NfcReader(uint8_t ssPin = PIN_RC522_SS,
                            uint8_t rstPin = PIN_RC522_RST,
                            uint8_t sckPin = PIN_RC522_SCK,
                            uint8_t misoPin = PIN_RC522_MISO,
                            uint8_t mosiPin = PIN_RC522_MOSI,
                            Print* log = nullptr)
        : mfrc522_(ssPin, rstPin),
          sckPin_(sckPin),
          misoPin_(misoPin),
          mosiPin_(mosiPin),
          ssPin_(ssPin),
          log_(log) {}

    bool begin() override {
        // Explicit pins: non-default wiring is changed in config.h only.
        // / Pines explícitos: cableado no estándar se cambia en config.h.
        SPI.begin(sckPin_, misoPin_, mosiPin_, ssPin_);
        return reinit(millis());
    }

    bool poll(std::string& uidOut) override {
        if (!healthy_) {
            // Reader not talking (failed init, or died later: wiring
            // glitch, ESD, brown-out). Retry on the configured cadence —
            // non-blocking, wrap-safe unsigned arithmetic.
            // / Lector sin respuesta. Reintento con la cadencia
            // configurada — no bloqueante, aritmética sin desbordamiento.
            const uint32_t now = millis();
            if (now - lastInitAttemptMs_ >= RC522_REINIT_INTERVAL_MS) {
                reinit(now);  // logs the recovery when it succeeds
            }
            return false;
        }

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

    /** Last PCD firmware-version byte seen (0 when never probed). */
    uint8_t lastVersion() const { return version_; }

private:
    /**
     * (Re)initialize the MFRC522 and probe its VersionReg. Valid versions:
     * 0x91 (v1.0), 0x92 (v2.0); 0x90 / 0x88 show on some clones. 0x00 /
     * 0xFF mean the chip is not answering (wiring or power).
     * /(Re)inicializa el MFRC522 y sondea su VersionReg.
     */
    bool reinit(uint32_t now) {
        lastInitAttemptMs_ = now;
        mfrc522_.PCD_Init();
        version_ = mfrc522_.PCD_ReadRegister(MFRC522::PCD_Register::VersionReg);
        healthy_ = (version_ != 0x00 && version_ != 0xFF);

        if (log_) {
            if (healthy_) {
                log_->print("[NFC] RC522 detected — firmware version 0x");
                log_->print(version_ < 0x10 ? "0" : "");
                log_->print(version_, HEX);
                log_->println(" / detectado");
            } else {
                log_->print("[NFC] RC522 NOT responding (version 0x");
                log_->print(version_ < 0x10 ? "0" : "");
                log_->print(version_, HEX);
                log_->print(") — check wiring (SCK ");
                log_->print(sckPin_);
                log_->print(" / MISO ");
                log_->print(misoPin_);
                log_->print(" / MOSI ");
                log_->print(mosiPin_);
                log_->print(" / SDA ");
                log_->print(ssPin_);
                log_->print(" / RST ");
                log_->print(PIN_RC522_RST);
                log_->print(") + power 3.3 V / revisa cableado y alimentacion 3.3 V");
                log_->print(" — retrying every ");
                log_->print(RC522_REINIT_INTERVAL_MS / 1000);
                log_->println(" s / reintentando");
            }
        }
        return healthy_;
    }

    MFRC522 mfrc522_;
    uint8_t sckPin_;
    uint8_t misoPin_;
    uint8_t mosiPin_;
    uint8_t ssPin_;
    Print* log_;                 // nullable diagnostics sink

    bool healthy_ = false;       // last probe answered?
    uint8_t version_ = 0;        // last VersionReg byte seen
    uint32_t lastInitAttemptMs_ = 0;
};

}  // namespace Presence
