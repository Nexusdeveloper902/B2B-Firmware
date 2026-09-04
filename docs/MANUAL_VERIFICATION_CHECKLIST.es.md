# Lista de Verificación Manual — Firmware del Lector (banco, placa real)

> Also available in: [English](MANUAL_VERIFICATION_CHECKLIST.md)

Este es el **protocolo de banco humano** para el firmware del lector. Todo
lo que hay aquí requiere el ESP32 físico + RC522 + LEDs/botón y un backend
B2B-Core en ejecución. Las ejecuciones de agentes no pueden verificar el
comportamiento de hardware (la compilación y las pruebas de lógica en el
host se verifican aparte — ver `.agent/RUNS/`).

**Preparación para cada sesión:**

1. B2B-Core en ejecución: `./run setup && ./run serve` (anota la IP LAN de
   la máquina, p. ej. `192.168.1.50`); el `api_key` del lector lo imprime
   `./run setup`.
2. `include/secrets.h` completado: `WIFI_SSID`, `WIFI_PASSWORD`,
   `API_BASE_URL` = `http://<IP-LAN>:8000`, `READER_API_KEY`.
3. Firmware flasheado: `pio run -e esp32dev -t upload && pio device monitor`
   (usa `esp32dev-mock` para manejar los toques escribiendo UIDs — los
   mismos pasos aplican, sustituyendo «escribe UID + Enter» por «toca la
   tarjeta»).

Recorre todos los puntos; marca CUMPLE / FALLA / BLOQUEADO con una nota.
Los prefijos del registro serial (`[NFC] [OK] [404] [409] [422] [401]
[NET] [ERR]`) reflejan los patrones LED.

---

## 1. Arranque en modo OPERACIÓN + indicación de reposo

- [ ] 1.1 Placa alimentada con el botón de modo SIN pulsar → el banner
      serial muestra `Mode (button at boot) / Modo (boton al arrancar):
      OPERATION / OPERACION`.
- [ ] 1.2 El LED de MODO muestra el patrón de reposo de operación: un
      destello corto cada ~2 s (latido simple).
- [ ] 1.3 El serial muestra `[WiFi] connected / conectado — IP: <ip>`.
- [ ] 1.4 El Wi-Fi tarda ≤ 15 s o imprime el mensaje de reintento en
      segundo plano (el dispositivo continúa igual — no se cuelga).

## 2. Modo operación — toque exitoso

- [ ] 2.1 Toca una tarjeta conocida y emparejada (usa un `credential_uid`
      impreso por el seeder de B2B-Core; en el build simulado, escríbelo
      + Enter).
- [ ] 2.2 LED de EVENTO: sólido ~1.5 s (patrón de éxito). El zumbador
      pita si está presente.
- [ ] 2.3 El serial imprime `[OK] event logged / evento registrado —
      <nombre> (<tipo>)`.
- [ ] 2.4 **Comprobación en el backend**: el evento aparece en el panel
      de administración de B2B-Core (o consultando la tabla de eventos) —
      el toque realmente llegó.

## 3. Modo operación — casos de fallo (el dispositivo debe seguir respondiendo)

- [ ] 3.1 Toca un UID desconocido → 2 destellos + serial `[404] Card not
      recognized / Tarjeta no reconocida`.
- [ ] 3.2 Apaga el Wi-Fi (o aléjate del alcance) → toque → 5 destellos
      rápidos + serial `[NET] network failure / fallo de red`; el bucle
      sigue corriendo (toques repetidos siguen respondiendo, el LED de
      MODO sigue parpadeando).
- [ ] 3.3 Pon una `READER_API_KEY` INCORRECTA en secrets.h, reflashea →
      toque → 6 destellos rápidos + `[401] reader key rejected / clave de
      lector rechazada`. Restaura la clave correcta después.
- [ ] 3.4 Detén el servidor del backend → toque → patrón `[NET]`;
      reinicia el backend → toca de nuevo → patrón de éxito (recuperación
      confirmada).
- [ ] 3.5 Apoya la MISMA tarjeta sobre el lector ~5 s → exactamente UN
      evento (antirrebote); retírala y vuelve a tocar tras >2 s → un
      segundo evento.

## 4. Arranque en modo EMPAREJAR + indicación de reposo distinta

- [ ] 4.1 Mantén el botón de modo mientras pulsas EN/RESET → el banner
      muestra `PAIRING / EMPAREJAR`.
- [ ] 4.2 El LED de MODO muestra el patrón de reposo de emparejamiento:
      dos destellos cortos cada ~2 s (claramente distinto del modo
      operación).
- [ ] 4.3 Toca una tarjeta emparejada SIN sesión de emparejamiento armada
      → 3 destellos + serial `[409] No pairing session active / No hay
      ninguna sesion...`.

## 5. Modo emparejar — emparejamiento exitoso

- [ ] 5.1 Arma una sesión desde el host del backend (token admin — ver
      `docs/API_INTEGRATION.es.md`):
      `curl -X POST http://<backend>/api/v1/admin/students/<id>/arm-pairing -H "Authorization: Bearer <PAT-admin>" -H "Accept: application/json"`
      → `{"status":"ok","student_id":<id>,"expires_at":"..."}` (ventana de 45 s).
- [ ] 5.2 Dentro de la ventana, toca una tarjeta NUEVA (nunca emparejada).
- [ ] 5.3 LED de EVENTO: sólido ~1.5 s; el serial imprime
      `[OK] card paired to / tarjeta emparejada con: <nombre del estudiante>`.
- [ ] 5.4 **Comprobación en el backend**: la tarjeta aparece en B2B-Core
      vinculada a ese estudiante (panel o BD), y la sesión pendiente queda
      consumida.
- [ ] 5.5 Toca la misma tarjeta otra vez (sesión ya consumida) → 3
      destellos + `[409]` (sin sesión activa) — el emparejamiento es de
      un solo uso.

## 6. Modo emparejar — tarjeta ya emparejada

- [ ] 6.1 Arma una sesión para el estudiante A. Toca la tarjeta que ya
      está vinculada al estudiante B (del §5 o del seeder).
- [ ] 6.2 4 destellos + serial `[422] Card already paired / La tarjeta ya
      esta emparejada`.
- [ ] 6.3 **Comprobación en el backend**: la tarjeta B sigue vinculada al
      estudiante B (no se reasigna). La sesión pendiente de A sigue armada
      (no consumida) — tocar una tarjeta nueva para A dentro de la ventana
      sigue funcionando.

## 7. Caducidad de la ventana de emparejamiento

- [ ] 7.1 Arma una sesión; espera > 45 s (pasado `expires_at`); toca una
      tarjeta nueva → 3 destellos + `[409]` (la sesión caducada se trata
      como inactiva).

## 8. Degradación y recuperación del Wi-Fi (en uso)

- [ ] 8.1 Con el dispositivo en reposo en modo operación, desactiva el AP
      ~30 s: el LED de MODO sigue parpadeando; sin reinicios ni cuelgues.
- [ ] 8.2 Reactiva el AP → en ~10–15 s el dispositivo se reconecta
      (reintento en segundo plano) → el siguiente toque tiene éxito sin
      reiniciar.
- [ ] 8.3 Confirma que un toque hecho DURANTE el apagón produjo
      retroalimentación `[NET]` y NO se fabricó ningún evento en el
      backend.

## 9. Estabilidad ante ciclos de energía

- [ ] 9.1 Apaga y enciende 3 veces seguidas; cada arranque llega al patrón
      de reposo en menos de 20 s y se mantiene estable 2 min.

---

**Registra tus resultados** (fecha, commit del firmware, CUMPLE/FALLA por
ítem, notas) en la sección de registro de banco de este mismo archivo —
solo anexando. Si un ítem falla, revisa primero el cableado de
`docs/HARDWARE_SETUP.es.md`, luego deja una nota estilo issue en
`.agent/OBSERVATIONS/`.

## Registro de banco

_(aún sin corridas de banco registradas — las ejecuciones de agentes no
tienen hardware; primera corrida de banco pendiente)_
