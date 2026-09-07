# Firmware de la Estación de Cámara — TASK-008 (ES)

La mitad de adquisición de imágenes de la estación de reciclaje: una
ESP32-CAM AI-Thinker (OV3660) que captura un JPEG cuando se dispara el
disparador y lo sube a B2B-Core. Fusionada desde la implementación de
referencia verificada (`ESP32-CAM-CV/firmware/esp32_cam` — init de
cámara, captura JPEG, comandos seriales y visualizador HTTP), añadiendo
el cableado de subida que exige esta tarea.

El firmware del lector (RC522, entorno `esp32dev`) queda intacto — ver
`docs/HARDWARE_SETUP.es.md` para ese dispositivo.

## Qué hace este firmware

| Comando serial | Acción |
|---|---|
| `ENTER` (línea vacía) | Captura un JPEG de alta resolución → `POST /api/v1/recycling/capture` (botella-primero: el backend retiene la imagen `awaiting_card`; **sin llamada al clasificador hasta que una tarjeta la resuelva** — puerta de costo, spec §4) |
| `a <credential_uid>` | `POST /api/v1/recycling/captures/<última>/associate` — resuelve la última captura con esa tarjeta (evento + clasificación + puntos, B2B-Core TASK-025) |
| `e <event_id>` | Arma modo tarjeta-primero: el PRÓXIMO `ENTER` captura y hace `POST /api/v1/recycling/classify` con ese event_id |
| `c` | Captura local sin subir (desarrollo/visualizador) |

ENTER nunca se dispara múltiple por repetición de tecla ni entrada
amortiguada (disciplina de líneas + intervalo de 2 s, testeado en el
host en `test/test_capture_trigger.cpp`). La tecla ENTER es el
disparador físico *temporal* — el futuro sensor IR la sustituye SOLO en
la costura `CaptureTrigger` (spec §37).

**Botón disparador:** un pulsador momentáneo entre **GPIO12 y GND** (sin
resistencia — pull-up interno) dispara exactamente lo mismo que ENTER
(antirrebote, una foto por pulsación, mantenerlo no repite). GPIO12 se
eligió porque el bus de cámara, el LED de flash y tu RC522
(13/14/15/2/4) están en otros pines — ver `config.h`. Nunca
conectes GPIO12 a HIGH (strapping de arranque).

**Zumbador:** ausente en este banco (`PIN_CAM_BUZZER -1`). GPIO4 es el RST del RC522 (verificado 2026-09-07), y un zumbador en reposo LOW lo mantendría en reset para siempre — sin zumbador hasta moverlo de GPIO4. El LED de flash comparte GPIO4 y destella con la actividad del RST — normal.

## Provisionamiento

1. `cp include/secrets.camera.h.example include/secrets.camera.h` y completa (archivo propio, separado del `secrets.h` del lector — distinta identidad en el backend):
   - `WIFI_SSID` / `WIFI_PASSWORD` — la red del colegio.
   - `API_BASE_URL` — el host de B2B-Core, p. ej. `http://192.168.1.50:8000`
     (arráncalo con `./run serve --host` para que sea alcanzable por Wi-Fi).
   - `READER_API_KEY` — una clave de lector de **reciclaje** tal como la
     imprime `./run setup` de B2B-Core. La estación de cámara ES una
     identidad de lector para el backend. Con el seed de demo hay
     exactamente un lector de reciclaje (`Demo Reader — Recycling`);
     compartir su clave entre la estación RC522 y la cámara funciona
     (eventos y capturas quedan en la misma fila de estación). Para
     producción, provisiona una fila `readers` dedicada para la cámara.
2. Compila, flashea, monitoriza:

   ```
   pio run -e esp32cam -t upload
   pio device monitor -e esp32cam
   ```

   `monitor_dtr=0` / `monitor_rts=0` ya están en `platformio.ini` —
   **consérvalos**: el circuito auto-download del AI-Thinker deja el
   chip en mal estado si el puerto serial se abre de otra forma.

## Flujo de banco contra un B2B-Core en ejecución (el guion exacto)

Prerrequisitos: B2B-Core sembrado y sirviendo (`./run setup && ./run serve`),
la estación de cámara en Wi-Fi, su monitor serial abierto.

1. **Botella-primero (spec §3 Caso B).** Coloca la botella a la vista,
   presiona **ENTER** en el monitor serial. Salida esperada (resumida):

   ```
   HIGH-RES CAPTURE / CAPTURA ALTA RESOLUCIÓN
   Captured: 1024x768 / Capturado: 1024x768
   [EN] Bottle-first capture: uploading image (no card yet)...
   [EN] capture: HTTP 200
   {"status":"ok","capture_id":12,"state":"awaiting_card","expires_in":300,"next_step":"present_card"}
   [EN] Backend capture id 12 — now tap a card ('a <credential_uid>').
   ```

   El estudiante toca su tarjeta; el operador escribe
   `a A1B2C3D4E5F6` (el `credential_uid` que imprimió el seeder — o
   léelo del log serial de la estación RC522). Esperado:

   ```
   [EN] associate: HTTP 200
   {"status":"ok","capture_id":12,"capture_state":"accepted",...,"material_class":"plastic","points_awarded":10,"new_balance":10}
   ```

   El tablero del panel y el escritorio del estudiante se actualizan en
   vivo (marcos WS de reciclaje, B2B-Core TASK-025 punto 6).

2. **Tarjeta-primero (spec §3 Caso A).** Toca una tarjeta en la estación
   RC522 y anota el `event_id` de su log serial; en el monitor de la
   CÁMARA escribe `e 88`, coloca la botella, presiona **ENTER**.
   Esperado: la subida de clasificación corre (`classify: HTTP 200`,
   `material_class`, `points_awarded`).

3. **Estados de fallo.** Un fallo de cámara/Wi-Fi/backend imprime
   `NETWORK ERROR (transport)` o el estado HTTP + el cuerpo JSON del
   backend — nunca un salto silencioso. Una captura sin tarjeta durante
   300 s simplemente expira en el servidor (sin premio, sin fuga —
   fijado por CaptureFlowTest de B2B-Core).

## Visualizador

Navega a la IP impresa al arranque: vista en vivo (`/stream-frame`), la
última captura de alta resolución (`/capture.jpg`, `/capture-status`) —
los ojos del operador de la referencia, conservados.

## Estado de verificación (sección de honestidad)

- Verificado en el host (este repositorio): 86/86 pruebas nativas, los
  cuatro entornos de compilación en verde (`esp32dev`, `esp32dev-mock`,
  `esp32cam`, y un checkout nuevo sin los archivos de secretos compila
  gracias a la guarda `__has_include`).
- Verificado en banco por el propietario: el flujo anterior, contra
  hardware real — según la convención de honestidad de hardware del
  repositorio (`docs/MANUAL_VERIFICATION_CHECKLIST.es.md`).
- Los endpoints del backend están fijados por la propia suite de
  B2B-Core (CaptureFlowTest + e2e Fase H) — ver `docs/API.md` de ese
  repositorio para los contratos exactos.
