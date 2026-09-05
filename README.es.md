# Plataforma de Presencia — Firmware del Lector (ESP32)

> También disponible en: [English](README.md)
> Repositorios companions: [B2B-Core](https://github.com/Nexusdeveloper902/B2B-Core) (backend — referencia de solo lectura salvo el endpoint de emparejamiento TASK-010)

Firmware ESP32 para los lectores físicos de tarjetas NFC de la Plataforma
de Presencia, construido como **proyecto PlatformIO** para el framework
**Arduino**. Cada lector hace una llamada HTTP autenticada al backend
B2B-Core — la misma llamada que antes enviaba una persona desde Postman.
El backend no requiere cambios para el hardware real (por diseño); la
única excepción, el endpoint de emparejamiento de tarjetas, se construyó
en B2B-Core como `TASK-010` específicamente para desbloquear este firmware.

## Qué hace

Dos modos de operación. El equipo arranca en **Operación**; el operador
pasa a Emparejar y vuelve en cualquier momento escribiendo la **clave de
modo** (guardada en el gitignored `secrets.h`) + Enter en el Monitor
Serial:

| Modo | Comportamiento |
|---|---|
| **Operación** (al arrancar) | Tocar una tarjeta emparejada → `POST /api/v1/events/tap` → evento de presencia + retroalimentación |
| **Emparejar** | Tocar una tarjeta nueva → `POST /api/v1/admin/cards/pair` → tarjeta vinculada al estudiante armado |

- **Cambio de modo**: contraseña por consola serial (`MODE_PASSWORD` en secrets.h) alterna OPERACIÓN <-> EMPAREJAR en ejecución — 2 parpadeos lentos del LED de EVENTO confirman el cambio; 3 contraseñas erróneas bloquean la consola 10 s (configurable). Los caracteres tecleados se muestran como `*`.

- **NFC**: RC522 por SPI — build por defecto desde TASK-002 (`pio run -t upload` flashea el lector real). El lector simulado por Serial sigue disponible como entorno opcional de desarrollo (`-e esp32dev-mock`).
- **Identidad**: clave Bearer estática `READER_API_KEY` — la clave ES la identidad del lector.
- **Retroalimentación**: LED de modo continuo + patrones del LED de eventos + zumbador opcional; cada resultado (éxito / 404 / 409 / 422 / 401 / fallo de red) tiene un patrón distinto y un registro serial bilingüe.
- **Resiliencia**: lecturas con antirrebote, bucle no bloqueante basado en `millis()`, conexión Wi-Fi acotada con reconexión en segundo plano, tiempos de espera HTTP acotados — el dispositivo nunca se cuelga ni se reinicia por un fallo.

## Requisitos

- [PlatformIO](https://platformio.org/) (`pip install platformio`)
- Una placa de desarrollo ESP32 (se asume `esp32dev` genérica) + módulo RC522 + LEDs (ver [docs/HARDWARE_SETUP.es.md](docs/HARDWARE_SETUP.es.md)); un equipo con el Monitor Serial para el cambio de modo
- Un backend [B2B-Core](https://github.com/Nexusdeveloper902/B2B-Core) en ejecución (`./run setup && ./run serve`) y el `api_key` del lector que imprime su seeder

## Inicio rápido

```bash
# 1. secretos (nunca se suben al repositorio)
cp include/secrets.h.example include/secrets.h
$EDITOR include/secrets.h      # WIFI_SSID, WIFI_PASSWORD, API_BASE_URL, READER_API_KEY, MODE_PASSWORD

# 2. compilar (lector real RC522 — entorno POR DEFECTO desde TASK-002)
#    cableado: SCK=18 MISO=19 MOSI=23 SDA/SS=5 RST=27 (docs/HARDWARE_SETUP.es.md)
pio run

# 3. pruebas unitarias en el host (sin hardware)
pio test -e native

# 4. flashear + monitor (un `pio run -t upload` simple flashea el lector real)
pio run -e esp32dev -t upload && pio device monitor
```

En un arranque sano el monitor imprime
`[NFC] RC522 detected — firmware version 0x92 / detectado` — la
confirmación positiva de que la radio del lector está viva. Si falta el
RC522 o está mal cableado, el firmware registra la versión sondeada y los
pines esperados y **sigue reintentando el init cada 5 s** (corrige el
cableado con el equipo encendido — sin reiniciar).

### ¿Sin lector conectado? Build de desarrollo (opcional)

```bash
pio run -e esp32dev-mock -t upload && pio device monitor
```

Con el **build simulado** flasheado (incluso en una placa sin nada conectado),
escribe un UID de tarjeta + Enter en el Monitor Serial para simular un
toque — se ejecuta todo el flujo.

### E2E de integración con el backend (sin hardware)

```bash
# contra un checkout local de B2B-Core (BD desechable + HTTP real)
B2B_CORE=../B2B-Core B2B_PHP=php ./scripts/e2e_backend.sh
```

El arnés envía los **payloads idénticos byte a byte** del firmware
(construidos por su propio `PayloadBuilder`) a un backend real en
ejecución, captura cada caso de respuesta documentado y pasa las
**respuestas reales** por el `ResponseParser` del firmware — 8/8
veredictos (tap + emparejar, incl. 404 / 409 / 422 / 401).

## Documentación

| Documento | Contenido |
|---|---|
| [docs/HARDWARE_SETUP.es.md](docs/HARDWARE_SETUP.es.md) | Cableado, pines, librerías, flasheo, tabla de patrones LED |
| [docs/API_INTEGRATION.es.md](docs/API_INTEGRATION.es.md) | El contrato exacto del backend que implementa este firmware (ambos modos, todos los casos de respuesta) |
| [docs/MANUAL_VERIFICATION_CHECKLIST.es.md](docs/MANUAL_VERIFICATION_CHECKLIST.es.md) | Lista de verificación de banco para una persona con la placa real |
| [.agent/](.agent/PROJECT.md) | Registros del protocolo de agentes (tareas, ADRs, ejecuciones, estado) |

## Estructura del repositorio

```text
platformio.ini            entornos de build: esp32dev (por defecto: RC522 real) | esp32dev-mock | native
include/config.h          pines + tiempos (confírmalos contra tu cableado)
include/secrets.h.example plantilla de credenciales (secrets.h está ignorado por git)
src/main.cpp              raíz de composición delgada (solo cableado)
lib/PresenceCore/         C++ puro: payloads, parsers, modos, antirrebote, patrones, consola serial
lib/NfcReader/            interfaz NfcReader + RC522 + simulador serial
lib/Feedback/             interfaz FeedbackController + implementación LED
lib/ApiClient/            interfaz ApiClient + transporte HTTPClient ESP32
lib/WifiService/          conexión acotada + reconexión no bloqueante
test/                     pruebas unitarias nativas en el host (48)
tools/e2e/                arnés E2E: payloads y parser del firmware vs backend real
scripts/e2e_backend.sh    E2E de integración con el backend (BD desechable, HTTP real)
docs/                     documentación real bilingüe (EN/ES)
```

## Honestidad del desarrollo

La mayor parte del desarrollo ocurre sin la placa conectada. Lo que se
verifica aquí: compilación para `esp32dev`, pruebas unitarias en el host
de toda la lógica independiente del hardware, y comportamiento del lector
simulado. Lo que requiere una persona en el banco: lecturas RC522 reales,
asociación Wi-Fi real, comportamiento de LEDs, tecleo en la consola
serial — guiado paso a paso por
[docs/MANUAL_VERIFICATION_CHECKLIST.es.md](docs/MANUAL_VERIFICATION_CHECKLIST.es.md).

## Seguridad

Las credenciales reales de Wi-Fi, URLs del backend y claves API de los
lectores nunca se suben al repositorio. `include/secrets.h` está ignorado
por git; solo la plantilla con marcadores se versiona.
