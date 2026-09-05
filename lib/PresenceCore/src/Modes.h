/**
 * Modes.h — the two concrete mode strategies.
 * Modes.h — las dos estrategias de modo concretas.
 *
 * OPERATION MODE: tap → POST /api/v1/events/tap → TapResult → feedback.
 * PAIRING MODE:   tap → POST /api/v1/admin/cards/pair → PairResult → feedback.
 *
 * Result interpretation lives HERE (not in main.cpp): each strategy reduces
 * its typed result into a FeedbackSignal, which is the ONLY thing the
 * FeedbackController consumes. / La interpretación de resultados vive AQUÍ
 * (no en main.cpp): cada estrategia reduce su resultado tipado a una
 * FeedbackSignal, único input del FeedbackController.
 */
#pragma once

#include "CoreTypes.h"
#include "Mode.h"

namespace Presence {

class OperationMode : public Mode {
public:
    ModeKind kind() const override { return ModeKind::Operation; }
    const char* label() const override { return "OPERATION / OPERACION"; }
    const char* hint() const override;
    ApiCallType callType() const override { return ApiCallType::Tap; }

    ApiCall onCardTap(const std::string& credentialUid) override;

    /** Reduce a parsed tap response into device feedback. */
    FeedbackSignal interpret(const TapResult& result) const;
};

class PairingMode : public Mode {
public:
    ModeKind kind() const override { return ModeKind::Pairing; }
    const char* label() const override { return "PAIRING / EMPAREJAR"; }
    const char* hint() const override;
    ApiCallType callType() const override { return ApiCallType::PairCard; }

    ApiCall onCardTap(const std::string& credentialUid) override;

    /** Reduce a parsed pairing response into device feedback. */
    FeedbackSignal interpret(const PairResult& result) const;
};

}  // namespace Presence
