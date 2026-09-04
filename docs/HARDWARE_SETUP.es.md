# Configuración de Hardware — Lector de la Plataforma de Presencia (ESP32 + RC522)

> Also available in: [English](HARDWARE_SETUP.md)

Este documento describe el cableado físico, los entornos de compilación y
el procedimiento de flasheo del firmware del lector. **Confirma cada pin
contra tu cableado real antes de confiar en él** — los valores por defecto
asumen un ESP32 DevKit genérico (`esp32dev`) y pueden cambiarse en
`include/config.h`.

## Lista de materiales

| Pieza | Notas |
|---|---|
| ESP32 DevKit (compatible esp32dev) | cualquier placa que exponga VSPI + GPIO libres |
| Módulo NFC RC522 | 13.56 MHz, interfaz SPI, 3.3 V |
| LED 1 — "MODO" | indica el modo de operación, de forma continua |
| LED 2 — "EVENTO" | reproduce los patrones de resultado de toque/emparejar |
| (opcional) zumbador pasivo | solo pitido de éxito |
| Botón momentáneo | a GND — mantenlo al alimentar para entrar en modo EMPAREJAR |
| 2 × resistencias (220–470 Ω) | resistencias en serie de los LEDs |
| Protoboard + cables | |

## Tabla de cableado

⚠ **Lee esto antes de cablear:** el RC522 es un dispositivo de 3.3 V —
aliméntalo desde el pin 3V3 del ESP32, nunca desde 5 V/VIN.

### RC522 (SPI — usa el bus VSPI del ESP32)

| Pin RC522 | GPIO ESP32 | constante en config.h |
|---|---|---|
| SDA (SS)  | **5**  | `PIN_RC522_SS` |
| SCK       | **18** | `PIN_RC522_SCK` (reloj VSPI) |
| MOSI      | **23** | `PIN_RC522_MOSI` (salida de datos VSPI) |
| MISO      | **19** | `PIN_RC522_MISO` (entrada de datos VSPI) |
| RST       | **27** | `PIN_RC522_RST` |
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

### Botón de selección de modo

| Conexión | constante en config.h | Significado |
|---|---|---|
| GPIO **32** ↔ botón ↔ GND | `PIN_MODE_SELECT` | `INPUT_PULLUP` activado |

Nivel lógico al arrancar (tras 50 ms de asentamiento / doble lectura):

| Nivel del pin al arrancar | Modo seleccionado |
|---|---|
| HIGH (botón sin pulsar) | **MODO OPERACIÓN** (toques normales) |
| LOW (botón pulsado al alimentar/pulsar EN/RESET) | **MODO EMPAREJAR** (emparejar tarjeta nueva) |

El modo queda fijo durante la sesión; para cambiarlo, mantén el botón y
pulsa EN/RESET. El LED de MODO siempre muestra el modo activo (ver tabla de
patrones), así que nunca tienes que adivinar.

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
| Respuesta inesperada del servidor | EVENTO | sólido largo 2 s |

El monitor serial imprime una línea bilingüe por cada evento (prefijos
`[NFC]` / `[OK]` / `[404]` / `[409]` / `[422]` / `[401]` / `[NET]` /
`[ERR]`), de modo que patrones y registro se confirman mutuamente.

## Entornos de compilación

| Entorno | Lector | Uso |
|---|---|---|
| `esp32dev` (**por defecto**) | `Rc522NfcReader` (RC522 por SPI) | el lector real — `pio run` y `pio run -t upload` lo compilan desde TASK-002 |
| `esp32dev-mock` (opcional) | `MockSerialNfcReader` — escribe un UID + Enter en el Monitor Serial | desarrollo sin RC522; aun así ejercita Wi-Fi, HTTP, modos y retroalimentación en una placa real |
| `native` | — | pruebas unitarias en el host (`pio test -e native`) |

## Flasheo

```bash
cp include/secrets.h.example include/secrets.h   # luego edita: Wi-Fi, URL del backend, clave del lector
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
[NFC] RC522 detected — firmware version 0x92 / detectado
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
| `MODE_BUTTON_SETTLE_MS` | 50 | antirrebote del botón al arrancar |

Todo el tiempo del bucle se basa en `millis()` y es no bloqueante; no hay
ningún `delay()` en `loop()`.
