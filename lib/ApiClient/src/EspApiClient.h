/**
 * EspApiClient.h — ApiClient over the ESP32 Arduino HTTPClient.
 * EspApiClient.h — ApiClient sobre HTTPClient de Arduino para ESP32.
 *
 * - Authorization: Bearer <READER_API_KEY> (the reader's whole identity)
 * - Content-Type: application/json
 * - Fixed timeout (config.h: HTTP_TIMEOUT_MS)
 * - Never throws; transport failures return status < 0 so the caller maps
 *   them to FeedbackKind::NetworkError and the device stays responsive.
 */
#pragma once

#include <HTTPClient.h>
#include <WiFi.h>

#include "ApiClient.h"

namespace Presence {

class EspApiClient : public ApiClient {
public:
    EspApiClient(const std::string& baseUrl, const std::string& bearerKey,
                 uint32_t timeoutMs = 10000)
        : baseUrl_(baseUrl), bearerKey_(bearerKey), timeoutMs_(timeoutMs) {}

    HttpResponse post(const std::string& path, const std::string& jsonBody) override {
        HttpResponse response;

        HTTPClient http;
        std::string url = baseUrl_ + path;

        if (!http.begin(url.c_str())) {
            response.status = -1;
            return response;
        }

        http.setTimeout(timeoutMs_);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Accept", "application/json");
        http.setAuthorization(bearerKey_.c_str());  // Bearer <key>

        int code = http.POST(jsonBody.c_str());
        response.status = code;

        if (code > 0) {
            response.transportOk = true;
            response.body = std::string(http.getString().c_str());
        } else {
            // HTTPClient error codes are negative (timeout, DNS, refused...).
            response.transportOk = false;
        }

        http.end();
        return response;
    }

private:
    std::string baseUrl_;
    std::string bearerKey_;
    uint32_t timeoutMs_;
};

}  // namespace Presence
