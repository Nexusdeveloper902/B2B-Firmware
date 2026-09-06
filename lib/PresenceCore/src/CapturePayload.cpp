#include "CapturePayload.h"

namespace Presence {

const char* CapturePayload::boundary() {
    // Literal chosen to be absent from JPEG entropy; fixed so payloads
    // and the Content-Type header can never disagree.
    return "----PresenceCamBoundary7C4F1A";
}

std::string CapturePayload::contentType() {
    return std::string("multipart/form-data; boundary=") + boundary();
}

std::string CapturePayload::imageOnly(const uint8_t* jpeg, size_t length) {
    return wrap("", jpeg, length);
}

std::string CapturePayload::classifyWithEvent(long eventId, const uint8_t* jpeg, size_t length) {
    std::string fields = "--";
    fields += boundary();
    fields += "\r\nContent-Disposition: form-data; name=\"event_id\"\r\n\r\n";
    fields += std::to_string(eventId);
    fields += "\r\n";
    return wrap(fields, jpeg, length);
}

std::string CapturePayload::wrap(const std::string& fields, const uint8_t* jpeg, size_t length) {
    std::string body = fields;

    body += "--";
    body += boundary();
    body += "\r\nContent-Disposition: form-data; name=\"image\"; filename=\"capture.jpg\"\r\n";
    body += "Content-Type: image/jpeg\r\n\r\n";

    body.append(reinterpret_cast<const char*>(jpeg), length);

    body += "\r\n--";
    body += boundary();
    body += "--\r\n";

    return body;
}

}  // namespace Presence
