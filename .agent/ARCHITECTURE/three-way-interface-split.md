# Architecture: the three-way interface split (firmware)

## Status
Architectural fact of the firmware repository (TASK-001; ADR-003).

## The split

The firmware's design center is three swappable interface families plus a
thin composition root:

```text
src/main.cpp (composition root — wiring only, no business logic)
   │
   ├── NfcReader            (lib/NfcReader)      HOW a UID is obtained
   │     ├── Rc522NfcReader       (esp32dev env)
   │     └── MockSerialNfcReader  (esp32dev-mock env, default)
   │
   ├── Mode                  (lib/PresenceCore)  WHAT a tap means
   │     ├── OperationMode   → POST /api/v1/events/tap
   │     └── PairingMode     → POST /api/v1/admin/cards/pair
   │
   └── FeedbackController    (lib/Feedback)      HOW results are shown
         └── LedFeedbackController (MODE LED + EVENT LED + buzzer)
```

Supporting pieces:
- `lib/PresenceCore` — pure C++ (no Arduino headers): PayloadBuilder,
  ResponseParser, Mode strategies, CardDebouncer, FeedbackPatterns.
  Everything here is host-testable in the `native` env.
- `lib/ApiClient` — `ApiClient` interface + `EspApiClient` (HTTPClient,
  Bearer reader key, bounded timeout).
- `lib/WifiService` — bounded connect-on-boot + periodic non-blocking
  reconnect.

## Flow of a tap (both modes, identical pipeline)

```text
reader.poll(uid) → debouncer.shouldProcess(uid, now)
  → mode->onCardTap(uid)                    // ApiCall {path, jsonBody}
  → api.post(path, jsonBody)                // HttpResponse {status, body}
  → ResponseParser::parse{Tap,Pair}Response // typed result
  → mode->interpret(result)                 // FeedbackSignal
  → feedback->showEvent(signal)             // LED/buzzer pattern
```

Every stage is an interface; every arrow is data. Business logic never
touches hardware registers, and hardware never knows HTTP.

## Design invariants

1. `lib/PresenceCore` contains ZERO Arduino includes (enforced by the
   native build: it cannot compile Arduino headers).
2. `src/main.cpp` contains no branching business rules — dispatch only.
3. The FeedbackSignal vocabulary (CoreTypes.h) is the ONLY channel from
   logic to presentation; adding a display means implementing
   FeedbackController, not touching modes.
4. The backend contract is encoded in exactly two places:
   PayloadBuilder (requests) and ResponseParser (responses) — locale-
   independent (HTTP status + `status` field decide, message text is
   display-only because the backend localizes via Accept-Language).
