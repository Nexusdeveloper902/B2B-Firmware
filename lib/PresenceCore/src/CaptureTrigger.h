/**
 * CaptureTrigger.h — the capture trigger boundary (TASK-008, spec §37).
 * CaptureTrigger.h — el límite del disparador de captura (TASK-008, spec §37).
 *
 * The physical trigger for "bottle placed, take the picture" is ENTER
 * over serial TODAY and will be an IR sensor TOMORROW; that swap must
 * touch ONLY this seam (the interface + its implementations), never the
 * capture/upload flow behind it.
 *
 * TerminalCaptureTrigger is the line-based implementation:
 *   ENTER (empty line) → CAPTURE        — the trigger itself
 *   "a <credential_uid>"  → ASSOCIATE   — resolve the last capture with a card
 *   "e <event_id>"        → ARM_EVENT   — card-first classify mode
 *   "c"                   → LOCAL_ONLY  — capture without uploading (dev)
 *
 * Multi-fire protection (spec: "Enter must not multi-fire on key-repeat
 * / buffered input"): the LineBuffer discipline turns a pasted char
 * blob into discrete lines, and a configurable cooldown (injectable
 * clock, host-testable) collapses key-repeat bursts of empty lines into
 * ONE capture. Pure C++ — no Arduino.
 * / El disparador físico es ENTER por serial HOY y será un sensor IR
 * mañana; ese cambio debe tocar SOLO esta costura. La disciplina de
 * LineBuffer convierte un pegado de caracteres en lineas discretas, y
 * un intervalo mínimo (reloj inyectable, testeable en host) colapsa las
 * repeticiones de Enter en UNA captura.
 */
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

namespace Presence {

/** What the terminal said. One Command per accepted line. */
struct CaptureCommand {
    enum Kind {
        None = 0,       // line produced no command (comment/unknown/cooldown)
        Capture,        // ENTER: capture + upload (the trigger)
        Associate,      // "a <uid>": resolve the last capture with this card
        ArmEvent,       // "e <id>": next capture classifies this event
        LocalOnly,      // "c": capture without uploading (dev)
    };

    // Explicit constructors: the Arduino toolchain compiles C++11, where
    // a struct with a default member initializer is not an aggregate and
    // brace-init fails to convert (the native env is newer and accepts
    // both — this keeps every env on the same code).
    CaptureCommand() = default;
    CaptureCommand(Kind k, std::string a) : kind(k), arg(std::move(a)) {}

    Kind kind = None;
    std::string arg;   // uid for Associate, decimal event id for ArmEvent
};

class CaptureTrigger {
public:
    virtual ~CaptureTrigger() = default;

    /** Feed one raw serial character; returns a command when a line
     *  completes into one (else .kind == None). */
    virtual CaptureCommand feed(char c) = 0;
};

class TerminalCaptureTrigger final : public CaptureTrigger {
public:
    using Clock = std::function<unsigned long()>;

    /**
     * @param clock         injectable millis() (host tests drive it)
     * @param cooldownMs    minimum spacing between CAPTURE commands —
     *                      the key-repeat guard. Default mirrors
     *                      CARD_COOLDOWN_MS semantics (2 s).
     * @param maxLineLength line cap, mirrors SERIAL_LINE_MAX_LENGTH
     */
    explicit TerminalCaptureTrigger(Clock clock, unsigned long cooldownMs = 2000,
                                    size_t maxLineLength = 64);

    CaptureCommand feed(char c) override;

    /** Trailing characters without a newline are NOT a command (a
     *  half-typed line must never fire on the next burst). */
    ~TerminalCaptureTrigger() override = default;

private:
    CaptureCommand handleLine(const std::string& line);

    Clock clock_;
    unsigned long cooldownMs_;
    size_t maxLineLength_;
    std::string line_;
    unsigned long lastCaptureAt_ = 0;
    bool everCaptured_ = false;
};

}  // namespace Presence
