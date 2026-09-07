# Configuración de Hardware — Lector de la Plataforma de Presencia (ESP32 + RC522)

> Also available in: [English](HARDWARE_SETUP.md)

Este documento describe el cableado físico, los entornos de compilación,
el procedimiento de flasheo y la consola serial de modo del firmware del
lector. **Confirma cada pin contra tu cableado real antes de confiar en
él** — los valores por defecto asumen un ESP32 DevKit genérico
(`esp32dev`) y pueden cambiarse en `include/config.h`.

## Lista de materiales

| Pieza | Notas |
|---|---|
| ESP32 DevKit (compatible esp32dev) | cualquier placa que exponga VSPI + GPIO libres |
| Módulo NFC RC522 | 13.56 MHz, interfaz SPI, 3.3 V |
| LED 1 — "MODO" | indica el modo de operación, de forma continua |
| LED 2 — "EVENTO" | reproduce los patrones de resultado de toque/emparejar |
| (opcional) zumbador pasivo | solo pitido de éxito |
| 2 × resistencias (220–470 Ω) | resistencias en serie de los LEDs |
| Protoboard + cables | |

> El BOTÓN de modo ya no forma parte de la lista: desde TASK-003 el
cambio de modo se hace por el Monitor Serial (contraseña) y no se cablea
ningún botón. El GPIO 32 queda libre. / The mode button is gone from the
BOM since TASK-003; GPIO 32 is free.

## Tabla de cableado

⚠ **Lee esto antes de cablear:** el RC522 es un dispositivo de 3.3 V —
aliméntalo desde el pin 3V3 del ESP32, nunca desde 5 V/VIN.

### RC522 (SPI — usa el bus VSPI del ESP32)

| Pin RC522 | GPIO ESP32 | constante en config.h |
|---|---|---|
| SDA (SS)  | **13** | `PIN_RC522_SS` |
| SCK       | **14** | `PIN_RC522_SCK` (reloj VSPI) |
| MOSI      | **15** | `PIN_RC522_MOSI` (salida de datos VSPI) |
| MISO      | **2**  | `PIN_RC522_MISO` (entrada de datos VSPI) |
| RST       | **4**  | `PIN_RC522_RST` (verificado en banco 2026-09-07: DIAG-CAM VersionReg 0x92 estable + raw coincide; GPIO16 es CS de PSRAM en ESP32-CAM — nunca RST) |
| 3.3V / VCC | 3V3  | — |
| GND       | GND   | — |

> ⚠ CONFIRMACIÓN DE CABLEADO: las cinco señales del RC522 son
> configurables en `include/config.h` (valores por defecto arriba).
> Algunos módulos RC522 y algunas placas ESP32 usan convenciones distintas
> (p. ej. SS=21, RST=22). Si el lector no se detecta, el registro serial
> imprime los pines esperados en la línea `[NFC] RC522 NOT responding
> (...)` — contrasta el cableado con esa línea y ajusta `config.h`.
>
> ✅ En un arranque sano el registro imprime `[NFC] RC522 detected —
> firmware version 0x92 / detectado` (0x91 = v1.0, 0x92 = v2.0,
> 0x90/0x88 = algunos clones). Esa línea es la confirmación positiva de
> que la radio está viva.

### Cambio de modo — contraseña en el Monitor Serial (TASK-003)

No requiere cableado. El equipo arranca en MODO OPERACIÓN; escribe la
contraseña de modo + Enter en el Monitor Serial (115200 baudios) para
alternar OPERACIÓN <-> EMPAREJAR en cualquier momento. El VALOR de la
contraseña vive en el gitignored `include/secrets.h` (`MODE_PASSWORD` —
ver `secrets.h.example`); los parámetros viven en `config.h`:

| Constante | Por defecto | Significado |
|---|---|---|
| `MODE_CONSOLE_MAX_WRONG_ATTEMPTS` | 3 | contraseñas erróneas antes del bloqueo |
| `MODE_CONSOLE_LOCKOUT_MS` | 10000 | duración del bloqueo |
| `SERIAL_LINE_MAX_LENGTH` | 64 | límite de longitud de línea (se descarta si excede) |

Conducta de la consola:

- Los caracteres tecleados se muestran como `*` (enmascarados: la
  contraseña nunca aparece en pantalla) y el registro nunca imprime la
  contraseña esperada.
- Contraseña correcta → `[MODE] switched to / cambiado a: ...` + 2
  parpadeos lentos del LED de EVENTO + el LED de MODO muestra de
  inmediato el patrón de reposo del nuevo modo.
- Contraseña errónea → `[MODE] wrong password / clave incorrecta — N
  intento(s) restante(s)` + 2 parpadeos muy rápidos. Tras 3 errores la
  consola se bloquea 10 s (`[MODE] input locked ...`), incluso para la
  contraseña correcta, y luego se restablece.
- Build con lector real: toda línea no vacía es un intento de contraseña.
- Build simulado: una línea que no es la contraseña es un toque virtual
  (UID), así que el bloqueo nunca se dispara al teclear UIDs normales.
- Honestidad de seguridad: el Monitor Serial es un canal de acceso físico
  (USB) — la contraseña es una compuerta de operador, no criptografía.
  Emparejar sigue requiriendo una sesión armada por un admin en el
  backend (ventana de 45 s) + la clave Bearer del lector.

### Salidas de retroalimentación

| Pieza | GPIO ESP32 | constante en config.h |
|---|---|---|
| LED MODO (ánodo) → resistencia → GPIO **25** | 25 | `PIN_LED_MODE` |
| LED EVENTO (ánodo) → resistencia → GPIO **26** | 26 | `PIN_LED_EVENT` |
| Zumbador (+) → GPIO **33** (opcional; pon `PIN_BUZZER` en `-1` si no hay) | 33 | `PIN_BUZZER` |

Cátodos de los LEDs → GND.

## Referencia de patrones LED

El LED de MODO parpadea CONTINUAMENTE para mostrar el estado; el LED de
EVENTO reproduce un patrón de una sola vez cada vez que se procesa una
tarjeta (y luego queda apagado).

| Significado | LED | Patrón |
|---|---|---|
| Arranque / conectando Wi-Fi | MODO | parpadeo rápido (100 ms encendido / 100 ms apagado) |
| Inactivo — modo OPERACIÓN | MODO | 1 destello corto cada 2 s |
| Inactivo — modo EMPAREJAR | MODO | 2 destellos cortos cada 2 s |
| Toque exitoso (evento registrado) | EVENTO | sólido 1.5 s |
| Emparejamiento exitoso (tarjeta vinculada) | EVENTO | sólido 1.5 s (+ pitido si hay zumbador) |
| Tarjeta no reconocida (404) | EVENTO | 2 destellos (200 ms) |
| Sin sesión de emparejamiento activa (409) | EVENTO | 3 destellos |
| Tarjeta ya emparejada (422) | EVENTO | 4 destellos |
| Fallo de red | EVENTO | 5 destellos rápidos (120 ms) |
| Fallo de autenticación (401, clave errónea) | EVENTO | 6 destellos rápidos (120 ms) |
| Modo cambiado (contraseña correcta) | EVENTO | 2 destellos lentos (500 ms encendido / 250 ms apagado) |
| Contraseña de modo errónea | EVENTO | 2 destellos muy rápidos (80 ms) |
| Respuesta inesperada del servidor | EVENTO | sólido largo 2 s |

El monitor serial imprime una línea bilingüe por cada evento (prefijos
`[NFC]` / `[OK]` / `[404]` / `[409]` / `[422]` / `[401]` / `[NET]` /
`[MODE]` / `[ERR]`), de modo que patrones y registro se confirman mutuamente.

## Entornos de compilación

| Entorno | Lector | Uso |
|---|---|---|
| `esp32dev` (**por defecto**) | `Rc522NfcReader` (RC522 por SPI) | el lector real — `pio run` y `pio run -t upload` lo compilan desde TASK-002 |
| `esp32dev-mock` (opcional) | `MockSerialNfcReader` — escribe un UID + Enter en el Monitor Serial | desarrollo sin RC522; aun así ejercita Wi-Fi, HTTP, modos y retroalimentación en una placa real |
| `native` | — | pruebas unitarias en el host (`pio test -e native`) |

## Flasheo

```bash
cp include/secrets.h.example include/secrets.h   # luego edita: Wi-Fi, URL del backend, clave del lector, MODE_PASSWORD
pio run -e esp32dev -t upload                     # lector real (entorno por defecto desde TASK-002)
#   equivalente a: pio run -t upload
pio run -e esp32dev-mock -t upload                # lector simulado (opcional)

pio device monitor                                # 115200 baudios
```

La clave del lector viene de la salida del seeder de B2B-Core (`./run setup`
en el repositorio del backend imprime el `api_key` de cada lector).

## Auto-recuperación del lector (TASK-002)

El driver del RC522 vigila su propia salud: si el init falla al arrancar
(cableado, alimentación) o el lector deja de responder en ejecución
(glitch, ESD, caída de tensión), el firmware reintenta `PCD_Init` cada 5 s
(`RC522_REINIT_INTERVAL_MS`) — no bloqueante y sin reiniciar. Corrige el
cableado con el equipo encendido y la línea `[NFC] RC522 detected ...`
aparecerá en el siguiente reintento. Líneas de arranque del build con
lector real:

```text
Reader impl / Implementacion: RC522 (SPI)
Mode / Modo: OPERATION / OPERACION
[NFC] RC522 detected — firmware version 0x92 / detectado
---- type the MODE PASSWORD + Enter to switch modes / escribe la
     CLAVE DE MODO + Enter para cambiar de modo (secrets.h) ----
---- present a card to the reader / presenta una tarjeta al lector ----
```

El build simulado en cambio imprime `---- type a UID + Enter ... ----`.

## Librerías (gestionadas por PlatformIO, `platformio.ini`)

| Librería | Versión | Propósito |
|---|---|---|
| `bblanchon/ArduinoJson` | ^7.0.0 | JSON de peticiones/respuestas (también testeable en host) |
| `miguelbalboa/rfid` | ^1.6.4 | driver MFRC522/RC522 (solo entorno `esp32dev`) |

## Constantes de tiempo (config.h)

| Constante | Por defecto | Significado |
|---|---|---|
| `WIFI_CONNECT_TIMEOUT_MS` | 15000 | asociación Wi-Fi inicial acotada |
| `WIFI_RECONNECT_INTERVAL_MS` | 10000 | cadencia de reintento en segundo plano tras una caída |
| `HTTP_TIMEOUT_MS` | 10000 | tiempo de espera HTTP por petición |
| `CARD_COOLDOWN_MS` | 2000 | ventana de antirrebote de relectura del mismo UID |
| `RC522_REINIT_INTERVAL_MS` | 5000 | cadencia de reintento de init del RC522 mientras no responde (auto-recuperación) |
| `MODE_CONSOLE_MAX_WRONG_ATTEMPTS` | 3 | contraseñas de modo erróneas antes del bloqueo |
| `MODE_CONSOLE_LOCKOUT_MS` | 10000 | duración del bloqueo de la consola de modo |
| `SERIAL_LINE_MAX_LENGTH` | 64 | límite de longitud de línea serial (se descarta si excede) |

Todo el tiempo del bucle se basa en `millis()` y es no bloqueante; no hay
ningún `delay()` en `loop()`.
