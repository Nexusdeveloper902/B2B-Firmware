# Pairing Mode — how a card meets its student
# Modo de emparejamiento (see [PAIRING.es.md](PAIRING.es.md) for Spanish)

> Also available in: [Español](PAIRING.es.md)
> Scope: the firmware side of card pairing. Backend contract source:
> [B2B-Core](https://github.com/Nexusdeveloper902/B2B-Core) —
> `TASK-010-card-pairing-endpoint`, ADR-020 (arm-then-pair).
> Related firmware docs: [API_INTEGRATION.md](API_INTEGRATION.md) (exact
> HTTP contract), [MANUAL_VERIFICATION_CHECKLIST.md](MANUAL_VERIFICATION_CHECKLIST.md)
> §4–§7 (bench tests), [HARDWARE_SETUP.md](HARDWARE_SETUP.md) (wiring).

Pairing mode is the **one supervised moment** in which a physical NFC card
becomes a student's credential. This document is the complete operator
guide: what the mode is, the exact flow, the prerequisites (including the
reader-key provisioning that a `[401]` points at), every response the
device can show, and how to troubleshoot each one.

---

## TL;DR — the flow at a glance

Pairing is **two steps, by design, in this order**:

```text
STEP 1 (backend, admin)                STEP 2 (device, any operator)
POST /api/v1/admin/students/{id}/      1. type the MODE_PASSWORD in the
     arm-pairing                          Serial Monitor → PAIRING mode
  → opens a 45 s window               2. tap a FRESH card within it
                                        3. [OK] card paired to: <student>
```

**Arming happens BEFORE tapping.** There is no way to pair a card from the
reader alone — that is the security model, not a missing feature (see
[Why arm-then-pair](#why-arm-then-pair-the-design-rationale)).

---

## What pairing mode IS — and what it is NOT

| Pairing mode **IS** | Pairing mode **is NOT** |
|---|---|
| The device half of a two-step, admin-authorized flow that links a **fresh** card UID to a student record | Device enrollment: the reader does **not** register itself with the backend. Its key is provisioned server-side (see [Prerequisites](#prerequisites-do-these-once)) |
| A desk workflow: an admin arms for a specific student, then the card is presented at the reader | Card reassignment: a card that already belongs to a student is **never** silently reassigned (`422`) |
| Available at runtime by typing the mode password (`MODE_PASSWORD` in secrets.h) — TASK-003 | A configuration mode: nothing about the device (Wi-Fi, key, endpoint) can be changed from the console |
| One pairing per armed session (one-shot), re-arm for the next student | A bulk provisioning tool: cards are paired one at a time, by a human decision |

---

## Why arm-then-pair (the design rationale)

The pairing flow was designed in B2B-Core (ADR-020) around three
properties, and the firmware deliberately preserves all of them:

1. **A device must never be able to self-authorize.** If a reader could
   register its own key or mint its own cards, any stolen or cloned box
   could enroll credentials into the school system. The reader key is
   therefore created **on the backend** and the card-to-student decision
   is made by an **admin account**, never by the device.
2. **The card-to-student link is a human decision with a short fuse.**
   Arming creates a pending pairing with a 45 s window
   (`PAIRING_WINDOW_SECONDS`, default 45). Long enough to walk from the
   desk to the reader; short enough that an armed session is not a
   standing invitation. When several pairings overlap, the **most
   recently armed** one wins — the desk flow is sequential by nature.
3. **Cards are never silently reassigned.** Any UID that already has a
   `cards` row is rejected with `422`, whatever its status; a replacement
   card is a **new** credential that gets paired like any fresh card.

---

## Prerequisites (do these once)

### 1. Backend running and seeded

On the B2B-Core host: `./run setup && ./run serve` (the setup command is
additive and idempotent — re-running it does **not** wipe data or rotate
keys; it re-prints the existing credentials). Note the LAN IP, e.g.
`192.168.1.6:8000`. The seeder creates the demo admin
(`admin@presence.test` / `password`) and four students.

### 2. READER_API_KEY registered on the backend — the `[401]` checklist

This is the step a `[401] reader key rejected` points at. The backend
authenticates the device by looking up **exactly** the
`Authorization: Bearer <READER_API_KEY>` value in the `readers` table —
the key IS the reader identity. If no row matches (typo, invented key,
re-created database), every call from this firmware returns `401`.

Pick **one** of these provisioning options:

**Option A — use the seeder-printed key (recommended for the bench).**
Re-run `./run setup` on the backend host; the DemoSeeder prints the
current reader table:

```text
 [EN] Readers — send as header: Authorization: Bearer <api_key>
 Reader / Lector            Type        active_event_type     api_key (Bearer)
 Demo Reader — Classroom/PAE classroom   CLASS_ATTENDANCE      9f2c...  (32 chars)
 Demo Reader — Recycling   recycling   RECYCLING_DEPOSIT     51ab...
```

Copy the `api_key` of the reader this physical device represents (use the
**classroom** one for an attendance reader) into `READER_API_KEY` in
`include/secrets.h`, rebuild, reflash. `./run setup` re-prints the **same**
keys for existing rows — it never rotates them.

**Option B — pin your own key onto the reader row (when you already wrote
a specific key in secrets.h).** On the backend host:

```bash
php artisan tinker
>>> App\Models\Reader::where('label', 'Demo Reader — Classroom/PAE')
...     ->first()->update(['api_key' => 'YOUR-KEY-FROM-secrets.h']);
```

**Option C — production provisioning.** Reader rows (and their keys) are
created server-side by the school's admin tooling — the firmware never
participates in that.

**Verify the key before touching firmware** (from any machine that
reaches the backend):

```bash
curl -i -X POST http://<backend>/api/v1/events/tap \
     -H "Authorization: Bearer <READER_API_KEY>" \
     -H "Content-Type: application/json" \
     -d '{"credential_uid": "A-SEEDED-UID"}'
```

A `401` here means the key itself is not provisioned (fix here first).
A `404 Card not recognized` means the key is **fine** — you simply used a
UID no card row has (expected with an invented UID). A `200` means both
the key and that seeded card are live.

### 3. Firmware side

- `include/secrets.h` filled: `WIFI_SSID`, `WIFI_PASSWORD`,
  `API_BASE_URL` (no trailing slash), `READER_API_KEY`, `MODE_PASSWORD`.
- Flashed and monitoring: `pio run -e esp32dev -t upload && pio device
  monitor`. Boot banner ends in OPERATION mode.

### 4. An admin token to arm with

Arming is `auth:sanctum` + `role:admin` — an admin **session** (dashboard)
or a **personal access token** (PAT). To mint a PAT on the backend host:

```bash
php artisan tinker
>>> App\Models\User::where('email', 'admin@presence.test')
...     ->first()->createToken('pairing-arm')->plainTextToken;
```

(Admins get `403`-free access; a teacher token is refused with `403`,
guests with `401` — the roles are enforced server-side.) To find the
student to arm for: the admin dashboard, or
`App\Models\Student::pluck('name', 'id')` in tinker.

---

## Walkthrough — the full happy path

Step by step, with the exact serial lines the firmware prints (TASK-004
builds print guidance lines after every status; earlier builds print the
first line only):

1. **Arm for the student** (backend host, within 45 s of the tap):

   ```bash
   curl -X POST http://192.168.1.6:8000/api/v1/admin/students/3/arm-pairing \
        -H "Authorization: Bearer <admin-PAT>" \
        -H "Accept: application/json"
   # → {"status":"ok","student_id":3,"expires_at":"2026-09-05T14:03:41.000000Z"}
   ```

2. **Switch the device to PAIRING** — type `MODE_PASSWORD` + Enter in the
   Serial Monitor (characters echo as `*`):

   ```text
   ********
   [MODE] switched to / cambiado a: PAIRING / EMPAREJAR
   [MODE] arm a session first (admin, 45 s window), then tap a FRESH card —
        docs/PAIRING.md / arma primero una sesion (admin, ventana de 45 s),
        luego acerca una tarjeta NUEVA
   ```

   The MODE LED changes to the pairing idle pattern (two blips every
   ~2 s); the EVENT LED plays two slow confirmation blinks.

3. **Tap a FRESH card** (never paired before — e.g. a blank MIFARE from
   the card sheet) within the window:

   ```text
   [NFC] card / tarjeta: 62041607
   [OK] card paired to / tarjeta emparejada con: Maria González
   ```

   EVENT LED solid ~1.5 s. On the backend: a new `cards` row
   (`credential_uid` 62041607 → student 3) exists, the pending pairing is
   consumed, and the pairing reader is recorded on it.

4. **Use it immediately** — switch back (`MODE_PASSWORD` + Enter) and tap
   the same card in OPERATION:

   ```text
   [MODE] switched to / cambiado a: OPERATION / OPERACION
   [MODE] tap a PAIRED card to log the event / acerca una tarjeta EMPAREJADA
        para registrar el evento
   ...
   [NFC] card / tarjeta: 62041607
   [OK] event logged / evento registrado — Maria (CLASS_ATTENDANCE)
   ```

   Newly paired cards are active: a tap works right away, no extra step.

5. **Pair the next student** — arm again (each session is one-shot) and
   repeat. Arming while a previous session is still open simply supersedes
   it (most recent wins).

---

## What every outcome means

`parsePairResponse` decides on the HTTP status; the backend `message` is
printed for the log only (localized server-side, EN by default — the
firmware never branches on message text).

| Serial line | HTTP | LED | Backend meaning | Cause → fix |
|---|---|---|---|---|
| `[OK] card paired to / tarjeta emparejada con: <name>` | 200 | solid 1.5 s | Card created and linked; session consumed | — |
| `[401] reader key rejected ...` + key-remediation lines | 401 | 6 fast blinks | No `readers` row matches the Bearer key | Key not provisioned / typo → [Prerequisites §2](#2-reader_api_key-registered-on-the-backend--the-401-checklist) |
| `[409] <message>` + arm-first lines | 409 | 3 blinks | No active session (none armed, expired after 45 s, or already consumed) | Arm **before** tapping, and re-arm after every success → [TL;DR](#tldr--the-flow-at-a-glance) |
| `[422] <message>` + fresh-card lines | 422 | 4 blinks | This UID already has a `cards` row | Use a fresh card — the session **stays armed**, retry immediately |
| `[NET] network failure / fallo de red` | transport | 5 fast blinks | Wi-Fi/backend unreachable (DNS, refused, timeout) | Check AP / backend; the device self-recovers, no reboot |
| `[ERR] unexpected / inesperado: <msg>` | 5xx / other | long solid | Server error or malformed body | Check backend logs; the device stays responsive |

---

## LED + buzzer patterns (pairing mode)

| Pattern (EVENT LED) | Meaning |
|---|---|
| 2 slow blinks (500 ms) | Mode switch accepted (`ModeSwitched`) |
| 2 very fast blinks (80 ms) | Mode switch rejected — wrong password (`ModeRejected`) |
| solid ~1.5 s | Card paired successfully |
| 3 blinks | 409 — no active pairing session |
| 4 blinks | 422 — card already paired (session stays armed) |
| 6 fast blinks | 401 — reader key rejected |
| 5 fast blinks | Network failure |
| long solid | Server / unknown error |

The **MODE LED** stays the always-visible source of truth for the current
mode: pairing idle = **two** short blips every ~2 s (operation idle =
one).

---

## Troubleshooting

**`[401]` on every tap/pair** — the key is not provisioned server-side.
Work [Prerequisites §2](#2-reader_api_key-registered-on-the-backend--the-401-checklist):
run the verification curl; if it 401s, fix the key (Option A or B) before
re-flashing. Also confirm no stray whitespace/quotes in the `#define
READER_API_KEY "..."` line — the value sent is the exact string between
quotes.

**`[409]` immediately after arming** — check the order (arm → tap within
45 s), that the arm call returned `{"status":"ok"}` for the right student,
and that nobody else's tap consumed the session (each is one-shot). If the
backend sets `PAIRING_WINDOW_SECONDS` lower than 45, the real window is
that value (the device's guidance text mirrors the default only).

**`[422]` with a brand-new card** — the UID is not actually fresh: it was
paired earlier (this bench, the seeder, or another reader). UIDs are
case-sensitive strings; `62041607` ≠ `62041607 ` (trailing space). To
start over for a student, pair a different physical card; to re-link the
SAME UID to another student, delete the old `cards` row server-side
(an explicit admin action — by design, never from the device).

**`[NET]` mid-pairing** — the armed window keeps running server-side
(45 s from arming, not from your tap). If the network recovers inside the
window, just tap again; otherwise re-arm. The device reconnects Wi-Fi in
the background on its own.

**Tap of a just-paired card 404s in OPERATION** — unexpected: cards are
created active. Check the response body of the pairing call (was it really
`200`?) and the card's `status` server-side.

---

## FAQ

**Can the reader pair itself with the backend (device enrollment)?**
No — deliberately. A device that can self-authorize is a rogue-reader
hole; keys are born server-side ([Option A/B/C](#prerequisites-do-these-once)).

**Who can arm a pairing?** Admins (Sanctum session or PAT). Teachers are
refused with `403`, guests with `401` — enforced in the backend, not in
this firmware.

**Why 45 seconds?** Long enough to walk from the desk to the reader and
tap; short enough to not leave standing invitations. It is the backend's
`PAIRING_WINDOW_SECONDS` (default 45) — the hint text on the serial
console mirrors the default, the backend is always authoritative.

**Is a session one-shot?** Yes — consumed by the first successful pair.
Arming again is the normal way to pair the next student (most recent
armed session wins on overlap).

**A card was lost / a student needs a replacement.** Pair a NEW physical
card the same way (arm → tap). The old card's row remains server-side
(inactive or not, it can never be silently reassigned).

**Do multiple readers matter?** No — any reader whose key the backend
accepts can complete the armed session. The pending pairing records which
reader consumed it.

**Does pairing mode change what the device does in OPERATION?** No. Modes
only select which endpoint a tap hits; identity, Wi-Fi and configuration
are untouched.

**Where is the pairing contract specified exactly?**
[API_INTEGRATION.md](API_INTEGRATION.md) (request/response tables) and
B2B-Core `docs/API.md` §arm-pairing / §cards-pair.

---

## Security notes

- **Arm-then-pair is human-in-the-loop authorization**: the card-to-student
  decision is an admin's, made on the backend, valid for one card within
  45 s. The reader is a card-typing terminal, not an authority.
- **The reader key is the device identity.** Rotate it server-side
  (`readers.api_key`) if a device is compromised; the firmware holds it
  only in the gitignored `secrets.h`.
- **The mode password (ADR-005) is an operator gate, not cryptography** —
  it prevents accidental mode changes over the USB console. It gates
  nothing server-side: pairing still requires an armed session.
- **No silent reassignment, no standing sessions, no client-supplied
  identity** — the three invariants the whole flow is built on.
