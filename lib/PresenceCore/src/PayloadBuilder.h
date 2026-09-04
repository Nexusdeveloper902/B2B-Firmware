/**
 * PayloadBuilder.h — constructs the JSON request bodies the B2B-Core API
 * expects. Pure C++ (ArduinoJson runs on host and device alike), so the
 * exact wire format is unit-tested on the host.
 * PayloadBuilder.h — construye los cuerpos JSON que espera la API de
 * B2B-Core. C++ puro (ArduinoJson funciona igual en host y dispositivo).
 */
#pragma once

#include <string>

namespace Presence {

/** POST /api/v1/events/tap body: {"credential_uid": "...", "client_timestamp": "..."} */
std::string buildTapPayload(const std::string& credentialUid,
                             const std::string& clientTimestampIso = "");

/** POST /api/v1/admin/cards/pair body: {"credential_uid": "..."} */
std::string buildPairPayload(const std::string& credentialUid);

}  // namespace Presence
