#include "PayloadBuilder.h"

#include <ArduinoJson.h>

namespace Presence {

std::string buildTapPayload(const std::string& credentialUid,
                             const std::string& clientTimestampIso) {
    JsonDocument doc;  // ArduinoJson 7 default document

    doc["credential_uid"] = credentialUid;
    if (!clientTimestampIso.empty()) {
        // Optional device clock, ISO 8601. Absent → backend uses server time.
        doc["client_timestamp"] = clientTimestampIso;
    }

    std::string out;
    serializeJson(doc, out);
    return out;
}

std::string buildPairPayload(const std::string& credentialUid) {
    JsonDocument doc;

    doc["credential_uid"] = credentialUid;

    std::string out;
    serializeJson(doc, out);
    return out;
}

}  // namespace Presence
