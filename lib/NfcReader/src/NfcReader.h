/**
 * NfcReader.h — the swappable NFC reader interface.
 * NfcReader.h — interfaz intercambiable del lector NFC.
 *
 * The concrete chip driver (RC522 over SPI) and the serial mock both
 * implement this interface, so mode/API/feedback logic never knows which
 * one is behind it. / El driver concreto (RC522 por SPI) y el simulador
 * serial implementan esta interfaz: la lógica de modos/API/feedback nunca
 * sabe cuál hay detrás.
 */
#pragma once

#include <string>

namespace Presence {

class NfcReader {
public:
    virtual ~NfcReader() = default;

    /** Initialize the reader hardware. False when init fails. */
    virtual bool begin() = 0;

    /**
     * Non-blocking poll for a card UID.
     * Returns true (and fills uidOut, uppercase hex string) when a card
     * was read this call. / Sondeo no bloqueante: true cuando se leyó una
     * tarjeta (uidOut = cadena hex mayúsculas).
     *
     * HCE integration: uidOut may instead hold an application-level HCE
     * credential id (from the SELECT AID + CHALLENGE exchange, never the
     * RF UID) — lastKind() tells which one the last successful poll
     * returned ("physical" or "hce").
     */
    virtual bool poll(std::string& uidOut) = 0;

    /** HOW the last successful poll's credential was captured. */
    virtual const char* lastKind() const { return "physical"; }

    /** Short label for the serial log. */
    virtual const char* label() const = 0;
};

}  // namespace Presence
