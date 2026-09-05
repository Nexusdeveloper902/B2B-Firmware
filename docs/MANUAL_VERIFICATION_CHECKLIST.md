# Manual Verification Checklist — Reader Firmware (bench, real board)

> También disponible en: [Español](MANUAL_VERIFICATION_CHECKLIST.es.md)

This is the **human bench protocol** for the reader firmware. Everything
here requires the physical ESP32 + RC522 + LEDs, a computer attached to
the Serial Monitor (mode switching needs it), and a running B2B-Core
backend. Agent runs cannot verify hardware behavior (compilation
and host-side logic tests are verified separately — see `.agent/RUNS/`).

**Setup for every session:**

1. B2B-Core running: `./run setup && ./run serve` (note the machine's LAN
   IP, e.g. `192.168.1.50`). Setup prints TWO tables you will need:
   the **cards** table (seeded `credential_uid`s — the only UIDs a tap
   recognizes) and the **readers** table (`api_key` per reader).
   Re-running setup re-prints the SAME values — it never rotates existing
   ones. Key provisioning + where to find a real `credential_uid`:
   [PAIRING.md](PAIRING.md) §Prerequisites.
2. `include/secrets.h` filled: `WIFI_SSID`, `WIFI_PASSWORD`,
   `API_BASE_URL` = `http://<LAN-IP>:8000`, `READER_API_KEY`,
   `MODE_PASSWORD`.
3. Firmware flashed: `pio run -e esp32dev -t upload && pio device monitor`
   (use `esp32dev-mock` instead to drive taps by typing UIDs — the same
   steps apply, substituting "type UID + Enter" for "tap card").

Work through every item; mark PASS / FAIL / BLOCKED with a note. The
serial log prefixes (`[NFC] [OK] [404] [409] [422] [401] [NET] [ERR]`)
mirror the LED patterns.

---

## 1. Boot in OPERATION mode + idle indication

- [ ] 1.1 Board powered → serial banner shows
      `Mode / Modo: OPERATION / OPERACION` (boot default; the mode button
      is gone since TASK-003).
- [ ] 1.2 MODE LED shows the operation idle pattern: one short blip every
      ~2 s (single heartbeat).
- [ ] 1.3 Serial shows `[WiFi] connected / conectado — IP: <ip>`.
- [ ] 1.4 Wi-Fi takes ≤ 15 s or prints the background-retry message
      (device continues either way — not hung).

## 2. Operation mode — successful tap

- [ ] 2.1 Tap a known, paired card (use a real seeded `credential_uid`
      from the **cards table** of the `./run setup` output — see
      [PAIRING.md](PAIRING.md) Prerequisites 1/2 for the exact place
      and a tinker lookup; in mock build, type it + Enter).
- [ ] 2.2 EVENT LED: solid ~1.5 s (success pattern). Buzzer chirps if
      present.
- [ ] 2.3 Serial prints `[OK] event logged / evento registrado — <name> (<type>)`.
- [ ] 2.4 **Backend check**: the event appears in the B2B-Core admin
      dashboard (or `GET` the events table) — the tap actually landed.

## 3. Operation mode — failure cases (device must stay responsive)

- [ ] 3.1 Tap an unknown UID → 2 blinks + serial `[404] Card not
      recognized` followed by the TASK-004 "unpaired card? switch to
      PAIRING…" remediation lines. (Send `Accept-Language`-style ES
      message only if the backend was configured so; EN default.)
- [ ] 3.2 Turn Wi-Fi off (or move out of range) → tap → 5 fast blinks +
      serial `[NET] network failure / fallo de red`; the loop keeps
      running (repeat taps keep answering, MODE LED keeps blinking).
- [ ] 3.3 Set a WRONG `READER_API_KEY` in secrets.h, reflash → tap → 6
      fast blinks + `[401] reader key rejected / clave de lector
      rechazada` followed by the TASK-004 key-provisioning remediation
      lines (they point at docs/PAIRING.md §Prerequisites). Restore the
      correct key afterward.
- [ ] 3.4 Stop the backend server → tap → `[NET]` pattern; restart the
      backend → tap again → success pattern (recovery confirmed).
- [ ] 3.5 Rest the SAME card on the reader ~5 s → exactly ONE event
      (debounce); remove and re-tap after >2 s → a second event.

## 4. Switch to PAIRING mode (serial password) + distinct idle indication

- [ ] 4.1 In the Serial Monitor, type the mode password (`MODE_PASSWORD`
      from secrets.h) + Enter → serial shows
      `[MODE] switched to / cambiado a: PAIRING / EMPAREJAR`, the
      TASK-004 `[MODE] arm a session first…` hint line, typed characters
      appear as `*`, and the EVENT LED plays 2 slow blinks.
- [ ] 4.2 MODE LED shows the pairing idle pattern: two short blips every
      ~2 s (clearly different from operation mode).
- [ ] 4.3 Tap a paired card WITHOUT any pairing session armed → 3 blinks
      + serial `[409] No pairing session active` followed by the TASK-004
      "arm a session first…" remediation lines.
- [ ] 4.4 Type a WRONG password three times → after the third, serial
      shows `[MODE] input locked ... ` and even the correct password is
      refused; wait 10 s → the correct password works again.
- [ ] 4.5 Type the mode password again after testing → mode returns to
      OPERATION (`[MODE] switched to ... OPERATION / OPERACION`).

## 5. Pairing mode — successful pairing

- [ ] 5.1 Arm a session for the student (admin decision on the backend).
      Recommended — the dashboard pairing desk (B2B-Core TASK-011):
      log in as admin → **Pair cards** nav page → **Arm pairing** on
      the student's row (live countdown + success line on the page;
      full walkthrough: [PAIRING.md](PAIRING.md) Prerequisites 4).
      Automation alternative (curl + admin PAT — how to mint the PAT:
      [PAIRING.md](PAIRING.md) Prerequisites 4):
      `curl -X POST http://<backend>/api/v1/admin/students/<id>/arm-pairing -H "Authorization: Bearer <admin-PAT>" -H "Accept: application/json"`
      → `{"status":"ok","student_id":<id>,"expires_at":"..."}` (45 s window).
- [ ] 5.2 Within the window, tap a FRESH (never-paired) card UID.
- [ ] 5.3 EVENT LED: solid ~1.5 s; serial prints
      `[OK] card paired to / tarjeta emparejada con: <student name>`.
- [ ] 5.4 **Backend check**: the card appears in B2B-Core linked to that
      student (dashboard or DB), and the pending pairing is consumed.
- [ ] 5.5 Tap the same card again (session now consumed) → 3 blinks +
      `[409]` (no session active) — the pairing is one-shot.

## 6. Pairing mode — already-paired card

- [ ] 6.1 Arm a session for student A. Tap the card that is already
      linked to student B (from §5 or the seeder).
- [ ] 6.2 4 blinks + serial `[422] Card already paired` followed by the
      TASK-004 "use a FRESH card — the session stays armed" remediation
      lines.
- [ ] 6.3 **Backend check**: card B remains linked to student B (not
      reassigned). The pending session for A remains armed (not consumed)
      — tapping a fresh card for A within the window still succeeds.

## 7. Pairing window expiry

- [ ] 7.1 Arm a session; wait > 45 s (past `expires_at`); tap a fresh
      card → 3 blinks + `[409]` (expired session treated as inactive).

## 8. Wi-Fi degradation and recovery (mid-use)

- [ ] 8.1 With the device idle in operation mode, disable the AP for
      ~30 s: MODE LED keeps blinking; no reboot/hang.
- [ ] 8.2 Re-enable the AP → within ~10–15 s the device reconnects
      (background retry) → next tap succeeds without a reboot.
- [ ] 8.3 Confirm a tap made DURING the outage produced `[NET]` feedback
      and NO backend event was fabricated.

## 9. Power-cycle stability

- [ ] 9.1 Power-cycle 3 times in a row; every boot reaches the idle
      pattern within 20 s and stays stable for 2 min.

---

**Record your results** (date, firmware commit, PASS/FAIL per item, notes)
in this file's bench-log section below — append-only. If an item fails,
check `docs/HARDWARE_SETUP.md` wiring first, then open an issue-style note
in `.agent/OBSERVATIONS/`.

## Bench log

_(no bench runs recorded yet — agent runs have no hardware; first bench
run pending)_
