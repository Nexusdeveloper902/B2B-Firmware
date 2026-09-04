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
| SCK       | **18** | VSPI por hardware (fijo) |
| MOSI      | **23** | VSPI por hardware (fijo) |
| MISO      | **19** | VSPI por hardware (fijo) |
| RST       | **27** | `PIN_RC522_RST` |
| 3.3V / VCC | 3V3  | — |
| GND       | GND   | — |

> ⚠ CONFIRMACIÓN DE CABLEADO: GPIO 5 / 27 son los valores por defecto de
> SS/RST. Algunos módulos RC522 y algunas placas ESP32 usan convenciones
> distintas (p. ej. SS=21, RST=22). Si el lector falla al inicializar (el
> registro serial imprime `[!] NFC reader init failed`), revisa primero
> estos dos pines y ajusta `include/config.h`.

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
| `esp32dev-mock` (por defecto) | `MockSerialNfcReader` — escribe un UID + Enter en el Monitor Serial | desarrollo sin RC522; aun así ejercita Wi-Fi, HTTP, modos y retroalimentación en una placa real |
| `esp32dev` | `Rc522NfcReader` (RC522 por SPI) | el lector real |
| `native` | — | pruebas unitarias en el host (`pio test -e native`) |

## Flasheo

```bash
cp include/secrets.h.example include/secrets.h   # luego edita: Wi-Fi, URL del backend, clave del lector
pio run -e esp32dev -t upload                     # lector real
pio run -e esp32dev-mock -t upload                # lector simulado
pio device monitor                                # 115200 baudios
```

La clave del lector viene de la salida del seeder de B2B-Core (`./run setup`
en el repositorio del backend imprime el `api_key` de cada lector).

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
| `MODE_BUTTON_SETTLE_MS` | 50 | antirrebote del botón al arrancar |

Todo el tiempo del bucle se basa en `millis()` y es no bloqueante; no hay
ningún `delay()` en `loop()`.
