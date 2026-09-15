/**
 * EspApiClient.h — ApiClient over the ESP32 Arduino HTTPClient.
 * EspApiClient.h — ApiClient sobre HTTPClient de Arduino para ESP32.
 *
 * - Authorization: Pulse-HMAC <kid>:<nonce>:<sig> (ADR-016): every POST
 *   is signed with the reader secret and a fresh esp_random nonce. The
 *   secret itself NEVER rides the wire — a hotspot capture holds one
 *   single-use signature, useless for impersonation or replay.
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
#include <esp_system.h>  // esp_random() — fresh signing nonce per request

#include "ApiClient.h"
#include "RequestSigner.h"

namespace Presence {

class EspApiClient : public ApiClient {
public:
    EspApiClient(const std::string& baseUrl, const std::string& deviceSecret,
                 uint32_t timeoutMs = 10000)
        : baseUrl_(baseUrl), deviceSecret_(deviceSecret), timeoutMs_(timeoutMs) {}

    /** TASK-013: point at a newly discovered backend without rebuilding.
     *  The compiled API_BASE_URL stays the boot fallback; discovery owns
     *  the runtime value. / Apunta a un backend redescubierto en caliente. */
    void setBaseUrl(const std::string& baseUrl) {
        baseUrl_ = baseUrl;
    }

    /** Effective base URL (discovered, or the compiled fallback). */
    const std::string& baseUrl() const {
        return baseUrl_;
    }

    HttpResponse post(const std::string& path, const std::string& jsonBody) override {
        return post(path, jsonBody, "application/json");
    }

    /** POST raw bytes (e.g. JSON bodies) with an explicit Content-Type.
     *  Signs the exact bytes sent. / POST de bytes con Content-Type
     *  explícito, firmando los bytes enviados. */
    HttpResponse post(const std::string& path, const std::string& body,
                      const std::string& contentType) {
        return doPost(path, body, contentType, body);
    }

    /** POST multipart image bytes signed over the multipart canonical
     *  (event_id + sha256(image)), NOT the wire bytes: PHP never sees raw
     *  multipart (php://input is empty), so the backend reconstructs the
     *  same canonical from the parsed upload. Callers build the canonical
     *  with CapturePayload::classifySigningBody()/captureSigningBody(). */
    HttpResponse postMultipart(const std::string& path, const std::string& body,
                               const std::string& contentType,
                               const std::string& signingBody) {
        return doPost(path, body, contentType, signingBody);
    }

    HttpResponse doPost(const std::string& path, const std::string& body,
                        const std::string& contentType, const std::string& signingBody) {
        HttpResponse response;

        HTTPClient http;
        std::string url = baseUrl_ + path;

        if (!http.begin(url.c_str())) {
            response.status = -1;
            return response;
        }

        http.setTimeout(timeoutMs_);
        http.addHeader("Content-Type", contentType.c_str());
        http.addHeader("Accept", "application/json");
        // ADR-016 + TASK-007: explicit Authorization VALUE — the literal
        // scheme lives in PresenceCore's RequestSigner (host-tested) so a
        // transport-only regression cannot hide here again. addHeader is
        // safe: setAuthorization() is never called, so HTTPClient's
        // built-in auth block stays empty and this is the single
        // Authorization header on the wire.
        // / VALOR de Authorization explicito y FIRMADO por peticion.
        http.addHeader("Authorization",
                       signedAuthorizationValue(path, signingBody).c_str());

        // Byte-array POST (not the const-char* overload): multipart bodies
        // carry binary JPEG bytes that may contain NULs. The bytes go
        // verbatim (CapturePayload documents the wire format).
        int code = http.POST(reinterpret_cast<uint8_t*>(const_cast<char*>(body.data())),
                             body.size());
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
    /** Fresh 128-bit nonce per request, hex. Replays 401 server-side. */
    static std::string freshNonce() {
        uint8_t raw[16];
        for (int i = 0; i < 4; i++) {
            const uint32_t r = esp_random();
            raw[4 * i] = static_cast<uint8_t>(r >> 24);
            raw[4 * i + 1] = static_cast<uint8_t>(r >> 16);
            raw[4 * i + 2] = static_cast<uint8_t>(r >> 8);
            raw[4 * i + 3] = static_cast<uint8_t>(r);
        }
        return Signer::toHex(raw, sizeof(raw));
    }

    /** Pulse-HMAC value over the signing body (raw bytes for JSON, the
     *  multipart canonical for image posts — see postMultipart). */
    std::string signedAuthorizationValue(const std::string& path,
                                          const std::string& body) const {
        return Signer::authorizationValue(deviceSecret_, "POST", path, freshNonce(), body);
    }

    std::string baseUrl_;
    std::string deviceSecret_;  // READER_API_KEY: signing key, never transmitted
    uint32_t timeoutMs_;
};

}  // namespace Presence
