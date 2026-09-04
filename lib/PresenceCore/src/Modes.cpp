#include "Modes.h"

#include "PayloadBuilder.h"

namespace Presence {

const char* modeKindToString(ModeKind kind) {
    return kind == ModeKind::Operation ? "operation" : "pairing";
}

ApiCall OperationMode::onCardTap(const std::string& credentialUid) {
    ApiCall call;
    call.type = ApiCallType::Tap;
    call.path = "/api/v1/events/tap";
    call.jsonBody = buildTapPayload(credentialUid);
    return call;
}

FeedbackSignal OperationMode::interpret(const TapResult& result) const {
    FeedbackSignal signal;
    signal.detail = result.message;

    switch (result.outcome) {
        case TapOutcome::Success:
            signal.kind = FeedbackKind::TapSuccess;
            signal.detail = result.studentFirstName;
            return signal;
        case TapOutcome::CardNotRecognized:
            signal.kind = FeedbackKind::TapRejected;
            return signal;
        case TapOutcome::AuthFailure:
            signal.kind = FeedbackKind::AuthError;
            return signal;
        case TapOutcome::NetworkError:
            signal.kind = FeedbackKind::NetworkError;
            return signal;
        case TapOutcome::ServerError:
        case TapOutcome::UnknownError:
        case TapOutcome::ValidationError:
        default:
            signal.kind = FeedbackKind::ServerError;
            return signal;
    }
}

ApiCall PairingMode::onCardTap(const std::string& credentialUid) {
    ApiCall call;
    call.type = ApiCallType::PairCard;
    call.path = "/api/v1/admin/cards/pair";
    call.jsonBody = buildPairPayload(credentialUid);
    return call;
}

FeedbackSignal PairingMode::interpret(const PairResult& result) const {
    FeedbackSignal signal;
    signal.detail = result.message;

    switch (result.outcome) {
        case PairOutcome::Success:
            signal.kind = FeedbackKind::PairSuccess;
            signal.detail = result.pairedStudentName;
            return signal;
        case PairOutcome::NoActiveSession:
            signal.kind = FeedbackKind::PairNoSession;
            return signal;
        case PairOutcome::AlreadyPaired:
            signal.kind = FeedbackKind::PairAlreadyPaired;
            return signal;
        case PairOutcome::AuthFailure:
            signal.kind = FeedbackKind::AuthError;
            return signal;
        case PairOutcome::NetworkError:
            signal.kind = FeedbackKind::NetworkError;
            return signal;
        case PairOutcome::ServerError:
        case PairOutcome::UnknownError:
        case PairOutcome::ValidationError:
        default:
            signal.kind = FeedbackKind::ServerError;
            return signal;
    }
}

}  // namespace Presence
