#include <unity.h>

#include <cstring>

#include <string>

#include "Modes.h"

// --- mode strategy: which call a tap becomes --------------------------------

void test_operation_mode_routes_tap_to_events_endpoint(void) {
    Presence::OperationMode mode;
    Presence::ApiCall call = mode.onCardTap("A1B2C3D4");

    TEST_ASSERT_EQUAL_INT(static_cast<int>(Presence::ApiCallType::Tap),
                          static_cast<int>(call.type));
    TEST_ASSERT_EQUAL_STRING("/api/v1/events/tap", call.path.c_str());
    TEST_ASSERT_NOT_NULL(strstr(call.jsonBody.c_str(), "\"credential_uid\""));
    TEST_ASSERT_NOT_NULL(strstr(call.jsonBody.c_str(), "A1B2C3D4"));
    TEST_ASSERT_NULL(strstr(call.jsonBody.c_str(), "client_timestamp"));
}

void test_pairing_mode_routes_tap_to_pair_endpoint(void) {
    Presence::PairingMode mode;
    Presence::ApiCall call = mode.onCardTap("A1B2C3D4");

    TEST_ASSERT_EQUAL_INT(static_cast<int>(Presence::ApiCallType::PairCard),
                          static_cast<int>(call.type));
    TEST_ASSERT_EQUAL_STRING("/api/v1/admin/cards/pair", call.path.c_str());
    TEST_ASSERT_NOT_NULL(strstr(call.jsonBody.c_str(), "\"credential_uid\""));
    TEST_ASSERT_NULL(strstr(call.jsonBody.c_str(), "client_timestamp"));
}

void test_mode_labels_are_bilingual(void) {
    Presence::OperationMode op;
    Presence::PairingMode pair;

    TEST_ASSERT_NOT_NULL(strstr(op.label(), "OPERATION"));
    TEST_ASSERT_NOT_NULL(strstr(op.label(), "OPERACION"));
    TEST_ASSERT_NOT_NULL(strstr(pair.label(), "PAIRING"));
    TEST_ASSERT_NOT_NULL(strstr(pair.label(), "EMPAREJAR"));
}

void test_mode_kind_polymorphism(void) {
    Presence::OperationMode op;
    Presence::PairingMode pair;
    Presence::Mode* modes[] = {&op, &pair};

    TEST_ASSERT_EQUAL_INT(static_cast<int>(Presence::ModeKind::Operation),
                          static_cast<int>(modes[0]->kind()));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Presence::ModeKind::Pairing),
                          static_cast<int>(modes[1]->kind()));
}

// --- result → feedback mapping (operation mode) ------------------------------

void test_operation_interpret_success(void) {
    Presence::OperationMode mode;
    Presence::TapResult r;
    r.outcome = Presence::TapOutcome::Success;
    r.studentFirstName = "Maria";

    Presence::FeedbackSignal s = mode.interpret(r);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Presence::FeedbackKind::TapSuccess),
                          static_cast<int>(s.kind));
    TEST_ASSERT_EQUAL_STRING("Maria", s.detail.c_str());
}

void test_operation_interpret_all_failure_kinds(void) {
    Presence::OperationMode mode;

    struct Case {
        Presence::TapOutcome in;
        Presence::FeedbackKind want;
    };
    const Case cases[] = {
        {Presence::TapOutcome::CardNotRecognized, Presence::FeedbackKind::TapRejected},
        {Presence::TapOutcome::AuthFailure, Presence::FeedbackKind::AuthError},
        {Presence::TapOutcome::NetworkError, Presence::FeedbackKind::NetworkError},
        {Presence::TapOutcome::ValidationError, Presence::FeedbackKind::ServerError},
        {Presence::TapOutcome::ServerError, Presence::FeedbackKind::ServerError},
        {Presence::TapOutcome::UnknownError, Presence::FeedbackKind::ServerError},
    };

    for (const Case& c : cases) {
        Presence::TapResult r;
        r.outcome = c.in;
        Presence::FeedbackSignal s = mode.interpret(r);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(c.want), static_cast<int>(s.kind));
    }
}

// --- result → feedback mapping (pairing mode) --------------------------------

void test_pairing_interpret_success(void) {
    Presence::PairingMode mode;
    Presence::PairResult r;
    r.outcome = Presence::PairOutcome::Success;
    r.pairedStudentName = "Maria González";

    Presence::FeedbackSignal s = mode.interpret(r);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Presence::FeedbackKind::PairSuccess),
                          static_cast<int>(s.kind));
    TEST_ASSERT_EQUAL_STRING("Maria González", s.detail.c_str());
}

void test_pairing_interpret_all_failure_kinds(void) {
    Presence::PairingMode mode;

    struct Case {
        Presence::PairOutcome in;
        Presence::FeedbackKind want;
    };
    const Case cases[] = {
        {Presence::PairOutcome::NoActiveSession, Presence::FeedbackKind::PairNoSession},
        {Presence::PairOutcome::AlreadyPaired, Presence::FeedbackKind::PairAlreadyPaired},
        {Presence::PairOutcome::AuthFailure, Presence::FeedbackKind::AuthError},
        {Presence::PairOutcome::NetworkError, Presence::FeedbackKind::NetworkError},
        {Presence::PairOutcome::ValidationError, Presence::FeedbackKind::ServerError},
        {Presence::PairOutcome::ServerError, Presence::FeedbackKind::ServerError},
        {Presence::PairOutcome::UnknownError, Presence::FeedbackKind::ServerError},
    };

    for (const Case& c : cases) {
        Presence::PairResult r;
        r.outcome = c.in;
        Presence::FeedbackSignal s = mode.interpret(r);
        TEST_ASSERT_EQUAL_INT(static_cast<int>(c.want), static_cast<int>(s.kind));
    }
}

// --- mode hints (TASK-004): the device teaches its own flow ---------------

void test_pairing_hint_teaches_arm_first_flow(void) {
    Presence::PairingMode pair;
    const char* hint = pair.hint();

    // EN half: arm first + window + fresh card
    TEST_ASSERT_NOT_NULL(strstr(hint, "arm a session first"));
    TEST_ASSERT_NOT_NULL(strstr(hint, "45 s window"));
    TEST_ASSERT_NOT_NULL(strstr(hint, "FRESH card"));
    // ES half: arma primero + ventana + tarjeta nueva
    TEST_ASSERT_NOT_NULL(strstr(hint, "arma primero"));
    TEST_ASSERT_NOT_NULL(strstr(hint, "tarjeta NUEVA"));
    // Points at the dedicated doc (the full walkthrough lives there)
    TEST_ASSERT_NOT_NULL(strstr(hint, "PAIRING.md"));
}

void test_mode_hints_bilingual_distinct_nonempty(void) {
    Presence::OperationMode op;
    Presence::PairingMode pair;

    // Both hints say something in both languages
    TEST_ASSERT_TRUE(strlen(op.hint()) > 0);
    TEST_ASSERT_TRUE(strlen(pair.hint()) > 0);
    TEST_ASSERT_NOT_NULL(strstr(op.hint(), "PAIRED"));
    TEST_ASSERT_NOT_NULL(strstr(op.hint(), "EMPAREJADA"));
    // And the two modes never share guidance text
    TEST_ASSERT_TRUE(strcmp(op.hint(), pair.hint()) != 0);
}

void runModeTests() {
    RUN_TEST(test_operation_mode_routes_tap_to_events_endpoint);
    RUN_TEST(test_pairing_mode_routes_tap_to_pair_endpoint);
    RUN_TEST(test_mode_labels_are_bilingual);
    RUN_TEST(test_mode_kind_polymorphism);
    RUN_TEST(test_pairing_hint_teaches_arm_first_flow);
    RUN_TEST(test_mode_hints_bilingual_distinct_nonempty);
    RUN_TEST(test_operation_interpret_success);
    RUN_TEST(test_operation_interpret_all_failure_kinds);
    RUN_TEST(test_pairing_interpret_success);
    RUN_TEST(test_pairing_interpret_all_failure_kinds);
}
