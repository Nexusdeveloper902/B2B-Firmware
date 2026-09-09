/**
 * LedFeedbackController.h — LED (+ optional buzzer) rendering of feedback.
 * LedFeedbackController.h — renderizado de retroalimentación por LED
 * (+ zumbador opcional).
 *
 * Two channels:
 *   MODE LED  (pinMode OUTPUT) — continuous pattern from FeedbackPatterns,
 *             loops forever, changed by indicate().
 *   EVENT LED (pinMode OUTPUT) — one-shot pattern from showEvent().
 *   BUZZER    (optional pin)   — mirrors the EVENT LED pattern; pass a
 *             negative pin to disable.
 *
 * All timing is non-blocking (PatternPlayer + tick()).
 */
#pragma once

#include <Arduino.h>

#include "FeedbackController.h"

namespace Presence {

/** Chirp length for the operation-success buzzer (was a magic 120). */
constexpr uint32_t BUZZER_CHIRP_MS = 120;

class LedFeedbackController : public FeedbackController {
public:
    LedFeedbackController(uint8_t modeLedPin, uint8_t eventLedPin, int8_t buzzerPin = -1)
        : modeLedPin_(modeLedPin), eventLedPin_(eventLedPin), buzzerPin_(buzzerPin) {}

    void begin() {
        pinMode(modeLedPin_, OUTPUT);
        pinMode(eventLedPin_, OUTPUT);
        if (buzzerPin_ >= 0) {
            pinMode(buzzerPin_, OUTPUT);
        }
        digitalWrite(modeLedPin_, LOW);
        digitalWrite(eventLedPin_, LOW);
        if (buzzerPin_ >= 0) {
            digitalWrite(buzzerPin_, LOW);
        }
    }

    void indicate(FeedbackKind continuousState) override {
        continuousState_ = continuousState;
        modePlayer_.start(modeLedPattern(continuousState), /*loop=*/true);
    }

    void showEvent(const FeedbackSignal& signal) override {
        eventPlayer_.start(eventLedPattern(signal.kind), /*loop=*/false);
        if (buzzerPin_ >= 0 && signal.kind == FeedbackKind::TapSuccess) {
            // short confirmation chirp for operation successes only.
            // Elapsed-style (start + elapsed compare): absolute
            // millis()+120 breaks at the ~49.7-day wrap (the chirp
            // would die at ~0 ms exactly at the wrap instant).
            buzzerStartMs_ = millis();
            digitalWrite(buzzerPin_, HIGH);
            buzzerActive_ = true;
        }
    }

    void tick(uint32_t nowMs) override {
        bool level = false;
        if (modePlayer_.step(nowMs, level)) {
            digitalWrite(modeLedPin_, level ? HIGH : LOW);
        }

        level = false;
        if (eventPlayer_.step(nowMs, level)) {
            digitalWrite(eventLedPin_, level ? HIGH : LOW);
        } else {
            digitalWrite(eventLedPin_, LOW);
        }

        if (buzzerPin_ >= 0 && buzzerActive_ &&
            nowMs - buzzerStartMs_ >= BUZZER_CHIRP_MS) {
            digitalWrite(buzzerPin_, LOW);
            buzzerActive_ = false;
        }
    }

private:
    uint8_t modeLedPin_;
    uint8_t eventLedPin_;
    int8_t buzzerPin_;
    FeedbackKind continuousState_ = FeedbackKind::BootConnecting;
    PatternPlayer modePlayer_;
    PatternPlayer eventPlayer_;
    uint32_t buzzerStartMs_ = 0;
    bool buzzerActive_ = false;
};

}  // namespace Presence
