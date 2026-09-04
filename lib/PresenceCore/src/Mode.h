/**
 * Mode.h — the strategy interface for the reader's two operating modes.
 * Mode.h — interfaz estratégica para los dos modos de operación del lector.
 *
 * The concrete strategies (see Modes.h) decide WHICH backend call a card
 * tap produces and HOW its response translates into a FeedbackSignal.
 * They contain no hardware code, so the whole mode machine is
 * host-testable. / Las estrategias concretas deciden QUÉ llamada produce
 * un toque de tarjeta y CÓMO se traduce su respuesta en una FeedbackSignal.
 * No contienen código de hardware: toda la máquina de modos es testeable.
 */
#pragma once

#include <string>

#include "CoreTypes.h"

namespace Presence {

class Mode {
public:
    virtual ~Mode() = default;

    virtual ModeKind kind() const = 0;

    /** Short bilingual-safe label for the serial log. */
    virtual const char* label() const = 0;

    /**
     * Build the HTTP call this mode makes for a scanned card UID.
     * Construye la llamada HTTP que este modo hace para un UID leído.
     */
    virtual ApiCall onCardTap(const std::string& credentialUid) = 0;

    /** Which call type this mode issues (for dispatching the response). */
    virtual ApiCallType callType() const = 0;
};

}  // namespace Presence
