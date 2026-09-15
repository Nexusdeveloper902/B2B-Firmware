# Flasheo — `scripts/flash.sh`

> Also available in: [English](FLASHING.md)

`scripts/flash.sh` es el envoltorio de flasheo por placa: un solo comando
para compilar y subir el firmware correcto a la placa correcta,
opcionalmente seguido del monitor serial. Envuelve

```bash
pio run -e <entorno> -t upload [--upload-port <puerto>] [argumentos pio extra...]
```

## Por qué existe

Los DevKits ESP32 (el lector RC522) y las placas ESP32-CAM (la estación
de cámara) exponen **puentes USB-UART idénticos**, así que el tipo de
placa NO se puede autodetectar desde el puerto serial. La bandera que
pasas ES el selector. Un `pio run -t upload` simple compila el
`default_envs` de PlatformIO (`esp32cam`) — flashear una imagen `esp32dev`
en una placa CAM maneja los pines del bus de cámara como SPI, el fallo
exacto de placa equivocada que estas banderas evitan (ver ADR-010).

## Mapa placa ↔ entorno

| Bandera corta | Valor `--board` / `--env` / `-e` | Entorno | Objetivo |
|---|---|---|---|
| *(sin bandera)* | `esp32cam` / `cam` | `esp32cam` | **Estación de reciclaje ESP32-CAM** — el predeterminado, igual que `default_envs` en `platformio.ini` (estación primero desde ADR-010) |
| `--esp32` / `--reader` | `esp32` / `reader` / `esp32dev` | `esp32dev` | Lector RC522 en DevKit ESP32 |
| `--cam-reader` | `cam-reader` / `esp32cam-reader` | `esp32cam-reader` | Placa ESP32-CAM que corre **solo el lector** — RC522 en los pines de la estación, LED integrado, sin código de cámara ([HARDWARE_SETUP.es.md](HARDWARE_SETUP.es.md#lector-sobre-una-placa-esp32-cam-esp32cam-reader)) |
| `--mock` | `mock` / `esp32dev-mock` | `esp32dev-mock` | Lector simulado — toques virtuales por Serial, desarrollo sin el RC522 conectado |

> Desde ADR-010 el predeterminado es la **estación de cámara**, no el
> lector. Quien use un DevKit de lector debe pasar `--esp32` explícito.

## Uso

```bash
./scripts/flash.sh                      # estación de cámara (esp32cam — predeterminado)
./scripts/flash.sh --esp32              # DevKit lector (esp32dev)
./scripts/flash.sh --reader             # igual que --esp32
./scripts/flash.sh --cam-reader -m      # lector sobre placa ESP32-CAM (esp32cam-reader), luego monitorea
./scripts/flash.sh --mock               # lector simulado (esp32dev-mock)
./scripts/flash.sh --board esp32cam     # forma larga (también: --env / -e)
./scripts/flash.sh --esp32 --port /dev/ttyUSB1
./scripts/flash.sh --esp32 --monitor    # flashea y luego abre el monitor serial
./scripts/flash.sh --cam -m             # flashea la estación y luego monitorea
./scripts/flash.sh --cam -- --upload-port COM3   # lo que siga a `--` se envía a pio
```

## Opciones

| Opción | Efecto |
|---|---|
| `--esp32` / `--reader` | apunta a `esp32dev` (lector RC522 real) |
| `--esp32cam` / `--cam` | apunta a `esp32cam` (estación de cámara — también el predeterminado sin bandera) |
| `--cam-reader` | apunta a `esp32cam-reader` (imagen de lector en placa ESP32-CAM, sin cámara) |
| `--mock` | apunta a `esp32dev-mock` (lector simulado) |
| `--board <nombre>` / `--env <nombre>` / `-e <nombre>` | forma larga de lo anterior: acepta `esp32` \| `reader` \| `esp32dev`, `esp32cam` \| `cam`, `cam-reader` \| `esp32cam-reader`, `mock` \| `esp32dev-mock` |
| `--port <puerto>` | envía `--upload-port <puerto>` a `pio run` (p. ej. `/dev/ttyUSB1`, `COM3`) |
| `-m` / `--monitor` | tras flashear bien, `exec pio device monitor -e <entorno>` (115200 baudios) |
| `--` | todo lo que sigue se añade tal cual al comando `pio run` |
| `-h` / `--help` | imprime el bloque de uso del propio script |
| env `DRY_RUN` | imprime el comando `pio` sin ejecutarlo — `DRY_RUN=1 ./scripts/flash.sh --esp32` (sin hardware) |

Las banderas/placas desconocidas salen con código 2 y una pista con los
nombres válidos.

## Notas del monitor

- El monitor corre **con la configuración del entorno objetivo**: para la
  estación de cámara (y `esp32cam-reader`) eso mantiene `monitor_dtr=0` / `monitor_rts=0`, que
  el circuito auto-download del AI-Thinker exige — nunca abras el puerto
  de la CAM con un `pio device monitor` simple sin entorno.
- La velocidad es 115200 (`monitor_speed` en `platformio.ini`).
- Dentro del monitor se interactúa según la placa: escribe la clave de
  modo (lector/estación) o un UID de tarjeta + Enter (simulado).

## Antes de flashear

- Los secretos deben estar provisionados para la placa objetivo: el
  lector y la estación usan `include/secrets.h` (desde
  `secrets.h.example`); la estación de cámara además usa
  `include/secrets.camera.h` (desde `secrets.camera.h.example`). Ver
  `docs/HARDWARE_SETUP.es.md` / `docs/CAMERA_STATION.es.md`.
- `pio` (PlatformIO) debe estar en el `PATH` — si no, el script imprime
  `platformio not found (pip install platformio)` y sale con 1.
- No elijas dos placas a la vez: usa una bandera corta o un valor
  `--board`.

## Solución de problemas

| Síntoma | Causa / arreglo |
|---|---|
| El script flashea la imagen equivocada | El predeterminado sin bandera es la **estación de cámara** — pasa `--esp32` para un DevKit de lector, `--mock` para un build de toques virtuales |
| `unknown board: ...` | Error de nombre — válidos: `esp32` \| `esp32cam` \| `mock` (y los alias de arriba) |
| La CAM no flashea / queda en mal estado tras el monitor | Asegúrate de que el entorno sea `esp32cam` (`--cam`) para que `monitor_dtr`/`monitor_rts` sigan en 0 |
| `platformio not found` | `pip install platformio` |

Para cableado, mapa de pines, secretos y la consola serial de modo, ver
[docs/HARDWARE_SETUP.es.md](HARDWARE_SETUP.es.md) (lector) y
[docs/CAMERA_STATION.es.md](CAMERA_STATION.es.md) (estación).
