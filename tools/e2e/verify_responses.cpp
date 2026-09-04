/**
 * verify_responses.cpp — E2E harness (host side).
 * verify_responses.cpp — arnés E2E (lado host).
 *
 * Reads the REAL HTTP responses captured from a running B2B-Core backend
 * (status + body pairs written by scripts/e2e_backend.sh) and runs them
 * through the firmware's own ResponseParser — asserting the device would
 * take the correct action for every documented case.
 * / Lee respuestas HTTP REALES capturadas de un backend B2B-Core en
 * ejecución y las pasa por el ResponseParser del firmware — verifica que
 * el dispositivo actuaría correctamente en cada caso documentado.
 *
 * Case file format (one per line): <name> <kind:tap|pair> <http_status> <bodyFile>
 * Body files must be UTF-8; the final line may omit a body file.
 *
 * Usage: verify_responses <cases_file> <expected: name=outcome,...>
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "ResponseParser.h"

namespace {

struct Case {
    std::string name;
    bool isTap = true;
    int status = 0;
    std::string body;
};

const char* tapOutcomeName(Presence::TapOutcome o) {
    switch (o) {
        case Presence::TapOutcome::Success: return "success";
        case Presence::TapOutcome::CardNotRecognized: return "not_recognized";
        case Presence::TapOutcome::AuthFailure: return "auth";
        case Presence::TapOutcome::ValidationError: return "validation";
        case Presence::TapOutcome::ServerError: return "server";
        case Presence::TapOutcome::NetworkError: return "network";
        default: return "unknown";
    }
}

const char* pairOutcomeName(Presence::PairOutcome o) {
    switch (o) {
        case Presence::PairOutcome::Success: return "success";
        case Presence::PairOutcome::NoActiveSession: return "no_session";
        case Presence::PairOutcome::AlreadyPaired: return "already_paired";
        case Presence::PairOutcome::AuthFailure: return "auth";
        case Presence::PairOutcome::ValidationError: return "validation";
        case Presence::PairOutcome::ServerError: return "server";
        case Presence::PairOutcome::NetworkError: return "network";
        default: return "unknown";
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <cases_file>\n", argv[0]);
        return 2;
    }

    // Cases file: one JSON-ish line per case:
    //   name|tap|status|/path/to/body  (|pair| for pairing)
    // Optional extra field after body path: |=<expectedOutcome>
    const std::string casesPath = argv[1];
    std::ifstream in(casesPath);
    if (!in) { std::fprintf(stderr, "cannot open %s\n", casesPath.c_str()); return 1; }

    int total = 0;
    int passed = 0;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::vector<std::string> parts;
        std::stringstream ss(line);
        std::string part;
        while (std::getline(ss, part, '|')) parts.push_back(part);
        if (parts.size() < 4) continue;

        const std::string& name = parts[0];
        const bool isTap = (parts[1] == "tap");
        const int status = std::atoi(parts[2].c_str());
        const std::string expected = parts.size() > 4 ? parts[4] : "";
        std::string body;
        if (!parts[3].empty() && parts[3] != "-") {
            std::ifstream bf(parts[3], std::ios::binary);
            std::ostringstream bs;
            bs << bf.rdbuf();
            body = bs.str();
        }

        ++total;
        std::string actual;
        std::string detail;

        if (isTap) {
            Presence::TapResult r = Presence::parseTapResponse(status, body);
            actual = tapOutcomeName(r.outcome);
            if (r.outcome == Presence::TapOutcome::Success) {
                detail = r.studentFirstName + "/" + r.eventType;
            } else {
                detail = r.message;
            }
        } else {
            Presence::PairResult r = Presence::parsePairResponse(status, body);
            actual = pairOutcomeName(r.outcome);
            if (r.outcome == Presence::PairOutcome::Success) {
                detail = r.pairedStudentName;
            } else {
                detail = r.message;
            }
        }

        if (expected == actual) {
            ++passed;
            std::printf("  PASS  %-22s -> %-14s [%s]\n", name.c_str(), actual.c_str(), detail.c_str());
        } else {
            std::printf("  FAIL  %-22s -> %-14s (expected %s) [%s]\n",
                        name.c_str(), actual.c_str(), expected.c_str(), detail.c_str());
        }
    }

    std::printf("\nfirmware-parser verdicts: %d/%d correct\n", passed, total);
    return passed == total ? 0 : 1;
}
