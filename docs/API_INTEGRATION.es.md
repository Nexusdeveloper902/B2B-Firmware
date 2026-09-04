# Integración con la API — qué llama el firmware y qué espera

> Also available in: [English](API_INTEGRATION.md)
> Fuente autoritativa: el repositorio
> [B2B-Core](https://github.com/Nexusdeveloper902/B2B-Core) —
> `routes/api.php` + controladores (este archivo refleja el contrato
> verificado el 2026-09-05 contra el `main` de B2B-Core).

Todas las llamadas son HTTP plano, sin estado y versionado — el Principio
de Abstracción de Hardware del backend: cualquier cosa que pueda hacer un
POST HTTP autenticado funciona igual, sea Postman, curl, una prueba o este
firmware.

## Convenciones comunes

- **URL base**: `API_BASE_URL` en `include/secrets.h` (p. ej.
  `http://192.168.1.50:8000` — sin barra final).
- **Autenticación**: `Authorization: Bearer <READER_API_KEY>`. La clave ES
  la identidad del lector; el backend jamás confía en un id de lector
  suministrado por el cliente. La clave la imprime el DemoSeeder de
  B2B-Core (`./run setup`).
- **Content-Type**: `application/json` (solo el endpoint de clasificación
  usa multipart — no lo usa este firmware; ver «Fuera de alcance»).
- **Localización**: el texto de los `message` de error lo localiza el
  backend vía `Accept-Language`. Por eso el firmware NUNCA decide por
  texto del mensaje — decide por código HTTP y campo `status`, y trata el
  mensaje solo como contenido del registro serial.
- **Tiempos de espera**: el firmware acota cada petición
  (`HTTP_TIMEOUT_MS`, 10 s por defecto); los fallos de transporte se
  mapean al patrón de fallo de red y el dispositivo sigue respondiendo.

## MODO OPERACIÓN — POST /api/v1/events/tap

Se dispara al tocar una tarjeta cuando el botón de modo NO se mantuvo al
arrancar.

**Petición** (construida por `Presence::buildTapPayload`):

```json
{
  "credential_uid": "A1B2C3D4",
  "client_timestamp": "2026-09-05T07:58:00-05:00"
}
```

- `credential_uid` — el UID leído (cadena hex mayúsculas para el RC522;
  cualquier texto en modo simulado). Obligatorio.
- `client_timestamp` — reloj del dispositivo ISO 8601 opcional; el
  firmware lo omite por ahora y deja que el servidor feche el evento (un
  reloj roto nunca debe perder el toque — el backend también degrada con
  elegancia).

**Respuestas y manejo del firmware** (parseo: `Presence::parseTapResponse`):

| HTTP | Significado en el backend | Resultado parseado | Retroalimentación |
|---|---|---|---|
| 200 | `{ "status": "ok", "event_id": 1042, "event_type": "CLASS_ATTENDANCE", "student_first_name": "Maria", "next_step": null }` | `TapOutcome::Success` | LED EVENTO sólido 1.5 s (+ registro serial con estudiante y tipo; `next_step == "awaiting_classification"` se registra, nada más — la clasificación está fuera de alcance) |
| 401 | clave Bearer faltante/inválida → `{ "status": "error", "message": "..." }` | `TapOutcome::AuthFailure` | 6 destellos rápidos |
| 404 | tarjeta desconocida (`Card not recognized`) o inactiva (`Card is not active`) | `TapOutcome::CardNotRecognized` | 2 destellos; mensaje registrado |
| 422 | error de validación | `TapOutcome::ValidationError` | sólido largo (estilo error de servidor) |
| 5xx | error inesperado del servidor | `TapOutcome::ServerError` | sólido largo |
| transporte | tiempo de espera / DNS / conexión rechazada | `TapOutcome::NetworkError` | 5 destellos rápidos |

Cualquier combinación no reconocida se parsea como `UnknownError` →
patrón sólido largo; el bucle continúa con normalidad (el dispositivo
nunca se bloquea ante una respuesta malformada).

## MODO EMPAREJAR — POST /api/v1/admin/cards/pair

Se dispara al tocar una tarjeta cuando el botón de modo SÍ se mantuvo al
arrancar. El endpoint se construyó en B2B-Core como
`TASK-010-card-pairing-endpoint` (diseño de dos pasos armar-y-emparejar;
ventana por defecto 45 s — ver ADR-020 de B2B-Core).

**Petición** (construida por `Presence::buildPairPayload`):

```json
{ "credential_uid": "A1B2C3D4" }
```

**Respuestas y manejo del firmware** (parseo: `Presence::parsePairResponse`):

| HTTP | Significado en el backend | Resultado parseado | Retroalimentación |
|---|---|---|---|
| 200 | `{ "status": "ok", "paired_student_name": "Maria González", "student_id": 3 }` | `PairOutcome::Success` | LED EVENTO sólido 1.5 s; el registro serial nombra al estudiante |
| 401 | clave Bearer faltante/inválida | `PairOutcome::AuthFailure` | 6 destello rápidos |
| 409 | sin sesión de emparejamiento activa → `{ "status": "error", "message": "No pairing session active" }` | `PairOutcome::NoActiveSession` | 3 destellos; mensaje registrado |
| 422 | tarjeta ya emparejada (o UID malformado) → `{ "status": "error", "message": "Card already paired" }` | `PairOutcome::AlreadyPaired` | 4 destellos; mensaje registrado |
| 5xx / otro | inesperado | `ServerError` / `UnknownError` | sólido largo |
| transporte | tiempo de espera / DNS / rechazo | `PairOutcome::NetworkError` | 5 destello rápidos |

### Armar una sesión de emparejamiento (lado backend, NO lo hace el firmware)

El emparejamiento solo tiene éxito mientras haya una sesión pendiente
armada para un estudiante. Ármala desde sesión de admin o token de acceso
personal:

```bash
# contra el backend B2B-Core en ejecución
curl -X POST http://<backend>/api/v1/admin/students/<id>/arm-pairing \
     -H "Authorization: Bearer <PAT-de-admin-o-sesion>" \
     -H "Accept: application/json"
# → { "status": "ok", "student_id": <id>, "expires_at": "..." }  (ventana de 45 s)
```

Luego toca la tarjeta nueva en el lector en MODO EMPAREJAR dentro de la
ventana.

## Fuera de alcance para este firmware (deliberadamente)

- `POST /api/v1/recycling/classify` (flujo de cámara/clasificación) — el
  `next_step: "awaiting_classification"` de la respuesta del tap solo se
  registra.
- `POST /api/v1/admin/readers/{id}/mode` (reetiquetado remoto del lector).
- Canje, consulta en lenguaje natural y todos los endpoints de dashboard.
