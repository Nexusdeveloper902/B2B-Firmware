/**
 * ApiClient.h — HTTP transport interface to the B2B-Core backend.
 * ApiClient.h — interfaz de transporte HTTP hacia el backend B2B-Core.
 *
 * The interface keeps the transport swappable (HTTPClient today, a future
 * TLS/queueing layer later) and gives tests an obvious seam. Identity is
 * always the static Bearer reader key from secrets.h — the backend never
 * trusts a client-supplied reader id.
 * / La interfaz mantiene el transporte intercambiable y da a las pruebas
 * una costura clara. La identidad siempre es la clave Bearer del lector.
 */
#pragma once

#include <string>

#include "CoreTypes.h"

namespace Presence {

struct HttpResponse {
    int status = 0;       // HTTP status code; < 0 → transport failure
    std::string body;     // raw response body (may be empty)
    bool transportOk = false;
};

class ApiClient {
public:
    virtual ~ApiClient() = default;

    /** POST a JSON body to baseUrl + path with the Bearer reader key. */
    virtual HttpResponse post(const std::string& path,
                              const std::string& jsonBody) = 0;
};

}  // namespace Presence
