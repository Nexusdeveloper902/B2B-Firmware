/**
 * PulseEndpoint.h — the discovered Pulse backend address (pure C++).
 * PulseEndpoint.h — la dirección descubierta del backend Pulse (C++ puro).
 *
 * PresenceCore stays FREE of Arduino headers so this contract is
 * host-testable (native env, test_pulse_endpoint.cpp). The Arduino-only
 * query itself lives in lib/PulseDiscovery (ESPmDNS) and maps its
 * results onto PulseCandidate below; the SELECTION policy lives here so
 * every device image shares it and the host tests pin it.
 * / PresenceCore sigue LIBRE de cabeceras de Arduino para que este
 * contrato sea testeable en el host. La consulta (solo Arduino) vive en
 * lib/PulseDiscovery (ESPmDNS) y mapea sus resultados a PulseCandidate;
 * la política de SELECCIÓN vive aquí para que toda imagen la comparta.
 *
 * DNS-SD contract (must match B2B-Core scripts/mdns/pulse.service and
 * the serve.sh advertisement):
 *   service _pulse._tcp.local, TXT version=1 protocol=1 api=/api.
 * The service NAME is the backend identity: any answer to that query is
 * a Pulse backend. The `protocol` TXT is a compatibility gate — a result
 * carrying protocol != "1" is rejected (a future wire change); a result
 * with no protocol TXT is accepted (minimal advertisements keep working).
 */
#pragma once

#include <cstdint>
#include <string>

namespace Presence {

// --- DNS-SD identity (shared with the backend advertisement) ----------------
#define PULSE_MDNS_SERVICE_NAME "pulse"    // queried as _pulse (+ proto below)
#define PULSE_MDNS_SERVICE_PROTO "tcp"     // queried as _tcp
#define PULSE_MDNS_LABEL "_pulse._tcp.local"  // log/operator label only
#define PULSE_DEFAULT_API_PORT 8000        // artisan serve default; TXT never carries it
#define PULSE_PROTOCOL_VERSION "1"         // TXT protocol=… this firmware speaks

/** One DNS-SD answer, already mapped off the Arduino types. */
struct PulseCandidate {
    std::string ip;            // dotted IPv4, e.g. "192.168.1.50"
    uint16_t port = 0;         // SRV port
    std::string protocolTxt;   // TXT protocol value, "" when not advertised
};

/** The endpoint the HTTP layer dials. Invalid = keep the compiled fallback. */
struct PulseEndpoint {
    std::string host;
    uint16_t port = 0;
    bool valid = false;

    std::string baseUrl() const {
        return "http://" + host + ":" + std::to_string(port);
    }

    std::string urlFor(const std::string& path) const {
        return baseUrl() + path;
    }

    /** Parse the compiled API_BASE_URL fallback ("http://host[:port][/…]"). */
    static PulseEndpoint fromBaseUrl(const std::string& baseUrl) {
        PulseEndpoint out;
        std::string rest = baseUrl;
        const std::string scheme = "http://";
        if (rest.compare(0, scheme.size(), scheme) == 0) {
            rest = rest.substr(scheme.size());
        }
        const size_t slash = rest.find('/');
        const std::string hostPort =
            (slash == std::string::npos) ? rest : rest.substr(0, slash);
        if (hostPort.empty()) {
            return out;
        }
        const size_t colon = hostPort.rfind(':');
        if (colon == std::string::npos) {
            out.host = hostPort;
            out.port = 80;
        } else {
            out.host = hostPort.substr(0, colon);
            const std::string portStr = hostPort.substr(colon + 1);
            if (out.host.empty() || portStr.empty()) {
                return out;
            }
            long port = 0;
            for (size_t i = 0; i < portStr.size(); ++i) {
                if (portStr[i] < '0' || portStr[i] > '9') {
                    return out;
                }
                port = port * 10 + (portStr[i] - '0');
            }
            if (port <= 0 || port > 65535) {
                return out;
            }
            out.port = static_cast<uint16_t>(port);
        }
        out.valid = true;
        return out;
    }
};

/** Compatibility gate: absent TXT stays accepted, anything but "1" is out. */
inline bool pulseProtocolCompatible(const std::string& protocolTxt) {
    return protocolTxt.empty() ||
           protocolTxt == PULSE_PROTOCOL_VERSION;
}

/**
 * Selection policy: first candidate with a usable IPv4, a non-zero port
 * and a compatible protocol TXT wins. Returns false when nothing qualifies.
 */
inline bool pickPulseEndpoint(const PulseCandidate* candidates, int count,
                              PulseEndpoint& out) {
    out = PulseEndpoint();
    if (candidates == 0 || count <= 0) {
        return false;
    }
    for (int i = 0; i < count; ++i) {
        const PulseCandidate& c = candidates[i];
        if (c.ip.empty() || c.ip == "0.0.0.0" || c.port == 0) {
            continue;
        }
        if (!pulseProtocolCompatible(c.protocolTxt)) {
            continue;
        }
        out.host = c.ip;
        out.port = c.port;
        out.valid = true;
        return true;
    }
    return false;
}

}  // namespace Presence
