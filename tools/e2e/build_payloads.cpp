/**
 * build_payloads.cpp — E2E harness (host side).
 * build_payloads.cpp — arnés E2E (lado host).
 *
 * Writes the EXACT JSON request bodies the ESP32 firmware sends, using
 * the firmware's own PayloadBuilder — so the backend is exercised with
 * byte-identical device payloads.
 * / Escribe los cuerpos JSON EXACTOS que envía el firmware del ESP32,
 * usando su propio PayloadBuilder — el backend recibe payloads idénticos
 * a los del dispositivo.
 *
 * Usage: build_payloads <tap_uid> <pair_uid> <out_dir>
 */
#include <cstdio>
#include <cstdlib>
#include <string>

#include "PayloadBuilder.h"

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: %s <tap_uid> <pair_uid> <out_dir>\n", argv[0]);
        return 2;
    }

    const std::string tapUid = argv[1];
    const std::string pairUid = argv[2];
    const std::string dir = argv[3];

    const std::string tapPayload = Presence::buildTapPayload(tapUid);
    const std::string pairPayload = Presence::buildPairPayload(pairUid);

    const std::string tapPath = dir + "/tap_payload.json";
    const std::string pairPath = dir + "/pair_payload.json";

    FILE* f = std::fopen(tapPath.c_str(), "wb");
    if (f == nullptr) { std::perror(tapPath.c_str()); return 1; }
    std::fwrite(tapPayload.data(), 1, tapPayload.size(), f);
    std::fclose(f);

    f = std::fopen(pairPath.c_str(), "wb");
    if (f == nullptr) { std::perror(pairPath.c_str()); return 1; }
    std::fwrite(pairPayload.data(), 1, pairPayload.size(), f);
    std::fclose(f);

    std::printf("tap  payload: %s\n", tapPayload.c_str());
    std::printf("pair payload: %s\n", pairPayload.c_str());
    return 0;
}
