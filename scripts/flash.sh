#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# flash.sh — flash wrapper with an explicit board override.
# flash.sh — flasheo con selección explícita de placa.
#
# Why this exists: ESP32 devkits and ESP32-CAMs expose identical USB-UART
# bridges, so the board type CANNOT be auto-detected reliably — the flag
# below IS the selector. No flag = station default (matches platformio.ini).
#
# Usage / Uso:
#   ./scripts/flash.sh                    # station (esp32cam, default)
#   ./scripts/flash.sh --esp32            # reader (esp32dev)
#   ./scripts/flash.sh --esp32cam         # camera station (esp32cam)
#   ./scripts/flash.sh --mock             # mock reader (esp32dev-mock)
#   ./scripts/flash.sh --board esp32cam --port /dev/ttyUSB0 --monitor
#   ./scripts/flash.sh --board esp32cam -- <extra pio args>
# ---------------------------------------------------------------------------
set -Eeuo pipefail

ENV="esp32cam"
PORT=""
MONITOR=0

usage() {
    sed -n '2,/^set -Eeuo pipefail$/p' "$0" | sed '$d; s/^# \?//'
}

while [ $# -gt 0 ]; do
    case "$1" in
        -h|--help) usage; exit 0 ;;
        --esp32|--reader) ENV="esp32dev"; shift ;;
        --esp32cam|--cam) ENV="esp32cam"; shift ;;
        --mock) ENV="esp32dev-mock"; shift ;;
        --board|--env|-e)
            case "${2:-}" in
                esp32|reader|esp32dev) ENV="esp32dev" ;;
                esp32cam|cam) ENV="esp32cam" ;;
                mock|esp32dev-mock) ENV="esp32dev-mock" ;;
                *) echo "unknown board: '${2:-}' (want: esp32 | esp32cam | mock)"; exit 2 ;;
            esac
            shift 2 ;;
        --port) PORT="${2:-}"; shift 2 ;;
        -m|--monitor) MONITOR=1; shift ;;
        --) shift; break ;;
        -*) echo "unknown flag: $1 (see --help)"; exit 2 ;;
        *) echo "unknown arg: $1 (see --help)"; exit 2 ;;
    esac
done

CMD=(pio run -e "$ENV" -t upload)
[ -n "$PORT" ] && CMD+=(--upload-port "$PORT")
CMD+=("$@")

# ponytail: DRY_RUN escape hatch so flag parsing is checkable without hardware.
if [ -n "${DRY_RUN:-}" ]; then printf '+ %s\n' "${CMD[*]}"; exit 0; fi

command -v pio >/dev/null || { echo "platformio not found (pip install platformio)"; exit 1; }

"${CMD[@]}"
[ "$MONITOR" = "1" ] && exec pio device monitor -e "$ENV" ${PORT:+--port "$PORT"}
