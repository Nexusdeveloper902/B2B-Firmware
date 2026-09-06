#include "CaptureTrigger.h"

#include <cstdlib>

namespace Presence {

TerminalCaptureTrigger::TerminalCaptureTrigger(Clock clock, unsigned long cooldownMs,
                                               size_t maxLineLength)
    : clock_(std::move(clock)), cooldownMs_(cooldownMs), maxLineLength_(maxLineLength) {}

CaptureCommand TerminalCaptureTrigger::feed(char c) {
    // Line discipline (the ModeConsole LineBuffer rules): '\r' is
    // ignored, '\n' terminates, overlong input is rejected as a whole
    // line — a garbled/pasted flood can never become a command.
    if (c == '\r') {
        return {};
    }

    if (c == '\n') {
        CaptureCommand cmd = handleLine(line_);
        line_.clear();
        return cmd;
    }

    if (line_.size() >= maxLineLength_) {
        // Overlong line: swallow it (still consuming) and reject the
        // whole line when Enter finally arrives — handled by the
        // overflow flag encoded as a poisoned line_ we drop on '\n'.
        if (line_.size() == maxLineLength_) {
            line_ += '\x01';  // mark: line already too long
        }
        return {};
    }

    line_ += c;
    return {};
}

CaptureCommand TerminalCaptureTrigger::handleLine(const std::string& raw) {
    // Trim surrounding whitespace (tolerant of terminal padding).
    size_t begin = raw.find_first_not_of(" \t");
    if (begin == std::string::npos) {
        // ENTER: the capture trigger — subject to the cooldown so a
        // held key or a buffered burst fires exactly once.
        unsigned long now = clock_ ? clock_() : 0;
        if (everCaptured_ && (now - lastCaptureAt_) < cooldownMs_) {
            return {};  // key-repeat guard: swallowed
        }
        lastCaptureAt_ = now;
        everCaptured_ = true;
        return {CaptureCommand::Capture, ""};
    }
    size_t end = raw.find_last_not_of(" \t");
    std::string line = raw.substr(begin, end - begin + 1);

    // Poisoned (overlong) lines are rejected wholesale.
    if (raw.find('\x01') != std::string::npos) {
        return {};
    }

    if (line == "c" || line == "C") {
        return {CaptureCommand::LocalOnly, ""};
    }

    if (line.size() >= 2 && (line[0] == 'a' || line[0] == 'A') && line[1] == ' ') {
        std::string uid = line.substr(2);
        // Trim inner padding (pasted lines can carry stray spaces) —
        // the backend compares UIDs exactly, so a padded upload would
        // 404 as an unknown card.
        size_t ub = uid.find_first_not_of(" \t");
        size_t ue = uid.find_last_not_of(" \t");
        if (ub != std::string::npos) {
            uid = uid.substr(ub, ue - ub + 1);
        }
        if (!uid.empty() && uid.find_first_of(" \t") == std::string::npos) {
            return {CaptureCommand::Associate, uid};
        }
        return {};
    }

    if (line.size() >= 2 && (line[0] == 'e' || line[0] == 'E') && line[1] == ' ') {
        std::string id = line.substr(2);
        // Decimal event ids only — anything else is noise, never a
        // half-parsed upload.
        if (!id.empty() && id.find_first_not_of("0123456789") == std::string::npos) {
            return {CaptureCommand::ArmEvent, id};
        }
        return {};
    }

    return {};  // unknown line — including the mode password, which the
                // reader firmware owns; the camera station has no modes.
}

}  // namespace Presence
