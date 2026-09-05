/**
 * EspApiClient.h — ApiClient over the ESP32 Arduino HTTPClient.
 * EspApiClient.h — ApiClient sobre HTTPClient de Arduino para ESP32.
 *
 * - Authorization: Bearer <READER_API_KEY> (the reader's whole identity)
 * - Content-Type: application/json
 * - Fixed timeout (config.h: HTTP_TIMEOUT_MS)
 * - Never throws; transport failures return status < 0 so the caller maps
 *   them to FeedbackKind::NetworkError and the device stays responsive.
 *
 * TASK-007: the Authorization header is built EXPLICITLY (the literal
 * "Bearer " prefix comes from PresenceCore's bearerAuthorizationValue,
 * pinned by host tests). HTTPClient::setAuthorization(key) must NOT be
 * used: it prefixes the value with the default authorization type
 * "Basic", the backend ignores "Authorization: Basic <key>", and every
 * real-hardware call answered 401 with a valid key until this fix.
 * / TASK-007: la cabecera Authorization se construye EXPLICITAMENTE (el
 * prefijo "Bearer " viene de bearerAuthorizationValue en PresenceCore,
 * fijado por pruebas del host). No usar setAuthorization(key): prefija
 * "Basic" y el backend ignoraba la cabecera — todo el hardware real
 * recibia 401 con una clave valida hasta esta correccion.
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
        // TASK-007: explicit Authorization VALUE — see the header comment.
        // addHeader is safe here: setAuthorization() is never called, so
        // HTTPClient's built-in auth block stays empty and this is the
        // single Authorization header on the wire.
        // / TASK-007: VALOR de Authorization explicito — ver la cabecera.
        // addHeader es seguro: nunca se llama a setAuthorization(), asi
        // que esta es la unica cabecera Authorization en el cable.
        http.addHeader("Authorization",
                       bearerAuthorizationValue(bearerKey_).c_str());

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
