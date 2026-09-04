# OBS-001: ArduinoJson 7 `| nullptr` silently breaks value reads

## Found
2026-09-05, TASK-001 Phase G (native host tests), firmware repository.

## Symptom
`deserializeJson()` returned `Ok`, but `doc["status"] | nullptr` evaluated
to null for a key that demonstrably existed. 8 host tests failed on the
200-parsing paths while the same bodies parsed fine in a standalone
program that used direct conversion.

## Root cause
In ArduinoJson 7.4.3 (and, by its semantics, the 7.x line generally), the
fallback operator `value | nullptr` does not behave like a "default when
absent" read: the returned pointer is null even when the member exists.
The correct patterns are:

```cpp
const char* s = doc["key"];          // implicit conversion, null when absent
// or
if (doc["key"].is<const char*>()) { ... }
```

## Impact & fix
`ResponseParser.cpp` (both tap and pair parsing, plus message extraction)
used `| nullptr` in four places — every success-path test failed until the
fix. String/number fallbacks (`doc["event_id"] | -1`,
`doc["event_type"] | ""`) are unaffected: the trap is specifically the
`nullptr` fallback form.

## Cross-repo note
Any future b2b-core-side JSON consumer in PHP is unaffected (Laravel uses
its own JSON handling); this observation is ArduinoJson-specific, but the
firmware and any future device-side tooling share it.

## Guard
Host tests (`test/test_responses.cpp`) lock the success-path parsing; a
regression to `| nullptr` would fail `test_tap_success` and
`test_pair_success` immediately.
