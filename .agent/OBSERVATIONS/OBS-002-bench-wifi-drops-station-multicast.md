# OBS-002: bench Wi-Fi drops station → host multicast, so `_pulse._tcp` discovery fell back

## Found
2026-09-16, bench (reader `hce.17`, 192.168.1.9; backend host
192.168.1.11 on a Realtek `rtw88_8822bu` USB Wi-Fi adapter). Diagnosed
from B2B-Core while working on its TASK-047.

## Symptom
Every boot printed:

`[DISC] no Pulse service (_pulse._tcp.local) — fallback … http://192.168.1.11:8000`

yet `avahi-browse -rt _pulse._tcp` on the host showed the service
correctly (port 8000, `protocol=1`).

## Root cause
The network delivers multicast in one direction only. Measured from
the host:

- **Host → ESP32 works.** A multicast query for `pulse-reader.local`
  was answered by the ESP32.
- **ESP32 → host is lost.** A 30 s passive listen on 5353 received
  nothing from any station, and `pulse-reader.local` did not resolve.
- **Wi-Fi power save is not the cause.** Disabling it on the host
  changed nothing.

`PulseDiscovery::discover()` sends its query by multicast. The query
never reached avahi, and avahi only answers, so the firmware never got
a response.

## Resolution (no firmware change)
B2B-Core ADR-067: `./run serve` also runs `scripts/mdns/announce.py`,
which sends unsolicited `_pulse._tcp` responses every second in the
direction that works. ESP-IDF's search accepts them during the query
window, but **only from source port 5353**; an ephemeral port was
ignored. Bench results, taken by resetting the ESP32 over
`/dev/ttyUSB0` and reading its boot log:

| Announcer | Boots | Discovered |
|---|---|---|
| off | 2 | 0 (fallback) |
| on, ephemeral port | 1 | 0 |
| on, port 5353 | 4 | 4 (`[DISC] Pulse backend / backend Pulse: http://192.168.1.11:8000`) |

## Takeaways
- `avahi-browse` on the backend host proves only that avahi
  **publishes**; it says nothing about whether stations' queries arrive.
  To test discovery, test from the device (serial `[DISC]` line).
- The compiled `API_BASE_URL` fallback masked the problem while the
  host IP happened to match it. On the demo hotspot the IP differs, so
  a broken discovery would fail every tap.
- Discovery still runs only at boot and after a transport failure. A
  device that fell back while the backend was down keeps the fallback
  until one of those happens.
