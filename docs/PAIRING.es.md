# Modo de emparejamiento — cómo una tarjeta conoce a su estudiante
# Pairing mode (ver [PAIRING.md](PAIRING.md) para inglés)

> También disponible en: [English](PAIRING.md)
> Alcance: el lado del firmware del emparejamiento de tarjetas. Fuente del
> contrato: [B2B-Core](https://github.com/Nexusdeveloper902/B2B-Core) —
> `TASK-010-card-pairing-endpoint`, ADR-020 (armar-then-pair).
> Documentos relacionados: [API_INTEGRATION.es.md](API_INTEGRATION.es.md)
> (contrato HTTP exacto), [MANUAL_VERIFICATION_CHECKLIST.es.md](MANUAL_VERIFICATION_CHECKLIST.es.md)
> §4–§7 (pruebas de banco), [HARDWARE_SETUP.es.md](HARDWARE_SETUP.es.md)
> (cableado).

El modo de emparejamiento es el **único momento supervisado** en el que una
tarjeta NFC física se convierte en la credencial de un estudiante. Este
documento es la guía completa del operador: qué es el modo, el flujo
exacto, los prerrequisitos (incluido el provisionamiento de la clave de
lector al que apunta un `[401]`), cada respuesta que el dispositivo puede
mostrar y cómo solucionar cada caso.

---

## TL;DR — el flujo de un vistazo

Emparejar son **dos pasos, por diseño, en este orden**:

```text
PASO 1 (backend, admin)               PASO 2 (dispositivo, cualquier operador)
POST /api/v1/admin/students/{id}/     1. escribe la CLAVE DE MODO en el
     arm-pairing                        Monitor Serial → modo EMPAREJAR
  → abre una ventana de 45 s          2. acerca una tarjeta NUEVA dentro de ella
                                        3. [OK] tarjeta emparejada con: <estudiante>
```

**Armar ocurre ANTES de acercar la tarjeta.** No existe forma de emparejar
una tarjeta desde el lector solo — ese es el modelo de seguridad, no una
función faltante (ver [Por qué armar-antes-de-emparejar](#por-qué-armar-antes-de-emparejar-la-razón-de-diseño)).

---

## Qué ES el modo de emparejamiento — y qué NO es

| El modo de emparejamiento **ES** | El modo de emparejamiento **NO ES** |
|---|---|
| La mitad del dispositivo de un flujo de dos pasos autorizado por un admin, que vincula un UID de tarjeta **nueva** a un estudiante | Registro del dispositivo: el lector **no** se registra solo ante el backend. Su clave se provisiona del lado del servidor (ver [Prerrequisitos](#prerrequisitos-hazlos-una-vez)) |
| Un flujo de escritorio: un admin arma para un estudiante concreto y luego se presenta la tarjeta al lector | Reasignación de tarjetas: una tarjeta que ya pertenece a un estudiante **nunca** se reasigna en silencio (`422`) |
| Disponible en ejecución escribiendo la clave de modo (`MODE_PASSWORD` en secrets.h) — TASK-003 | Un modo de configuración: nada del dispositivo (Wi-Fi, clave, endpoint) se cambia desde la consola |
| Un emparejamiento por sesión armada (de un solo uso); re-arma para el siguiente estudiante | Una herramienta masiva: las tarjetas se emparejan de a una, por decisión humana |

---

## Por qué armar-antes-de-emparejar (la razón de diseño)

El flujo se diseñó en B2B-Core (ADR-020) alrededor de tres propiedades, y
el firmware las conserva deliberadamente:

1. **Un dispositivo nunca debe poder autoautorizarse.** Si un lector
   pudiera registrar su propia clave o acuñar sus propias tarjetas,
   cualquier caja robada o clonada podría inscribir credenciales en el
   sistema escolar. La clave del lector nace **en el backend** y la
   decisión tarjeta-estudiante la toma una **cuenta de admin**, nunca el
   dispositivo.
2. **El vínculo tarjeta-estudiante es una decisión humana con mecha
   corta.** Armar crea un emparejamiento pendiente con una ventana de 45 s
   (`PAIRING_WINDOW_SECONDS`, por defecto 45). Suficiente para caminar
   del escritorio al lector; demasiado corto para ser una invitación
   permanente. Si varios emparejamientos se solapan, gana el **armado más
   reciente** — el flujo de escritorio es secuencial por naturaleza.
3. **Las tarjetas nunca se reasignan en silencio.** Todo UID que ya tenga
   una fila en `cards` se rechaza con `422`, sea cual sea su estado; una
   tarjeta de reemplazo es una credencial **nueva** que se empareja como
   cualquier tarjeta fresca.

---

## Prerrequisitos (hazlos una vez)

### 1. Backend corriendo y sembrado

En el host de B2B-Core: `./run setup && ./run serve` (setup es aditivo e
idempotente — re-ejecutarlo **no** borra datos ni rota claves; re-imprime
las credenciales existentes). Anota la IP de LAN, p. ej.
`192.168.1.6:8000`. El seeder crea el admin demo
(`admin@presence.test` / `password`) y cuatro estudiantes.

El setup imprime **dos tablas que sí vas a necesitar** — la tabla de
**tarjetas** (los `credential_uid`: los ÚNICOS UIDs que el endpoint de
toque reconoce) y, justo debajo, la tabla de **lectores** (las claves
Bearer). Los `credential_uid` sembrados son **cadenas aleatorias de 12
caracteres en mayúsculas, una por estudiante** — se generan en la primera
siembra y permanecen estables en re-ejecuciones (firstOrCreate). Forma
de ejemplo (tus valores serán distintos):

```text
 [EN] Cards — use credential_uid as {"credential_uid": "..."} in POST /api/v1/events/tap
 [ES] Tarjetas — usa credential_uid como {"credential_uid": "..."} en POST /api/v1/events/tap
 Student / Estudiante        credential_uid
 Maria González              M9TN530AIT7N
 Carlos Pérez                4K2P81DXR7WQ
 ...
 [EN] Readers — send as header: Authorization: Bearer <api_key>
 [ES] Lectores — envía como cabecera: Authorization: Bearer <api_key>
 Reader / Lector             Type        active_event_type      api_key (Bearer)
 Demo Reader — Classroom/PAE classroom   CLASS_ATTENDANCE       9f2c...  (32 chars)
 Demo Reader — Recycling     recycling   RECYCLING_DEPOSIT      51ab...
```

> Conserva ambas tablas en el scrollback de tu terminal (o re-ejecuta
> `./run setup` más tarde — se imprimen los mismos valores). La clave va
> en `READER_API_KEY`; un `credential_uid` es lo que alimenta el curl de
> verificación de abajo y cualquier prueba manual de toque.

### 2. READER_API_KEY registrada en el backend — la lista de chequeo del `[401]`

Este es el paso al que apunta un `[401] clave de lector rechazada`. El
backend autentica el dispositivo buscando **exactamente** el valor de
`Authorization: Bearer <READER_API_KEY>` en la tabla `readers` — la clave
ES la identidad del lector. Si ninguna fila coincide (error de tipeo,
clave inventada, base de datos recreada), toda llamada de este firmware
devuelve `401`.

Elige **una** de estas opciones de provisionamiento:

**Opción A — usa la clave impresa por el seeder (recomendada en el banco).**
Re-ejecuta `./run setup` en el host del backend; el DemoSeeder imprime la
tabla de lectores actual:

```text
 [ES] Lectores — envía como cabecera: Authorization: Bearer <api_key>
 Lector / Lector         Tipo       active_event_type     api_key (Bearer)
 Demo Reader — Classroom/PAE classroom  CLASS_ATTENDANCE      9f2c...  (32 chars)
 Demo Reader — Recycling  recycling  RECYCLING_DEPOSIT     51ab...
```

Copia el `api_key` del lector que representa este dispositivo físico (usa
el de **classroom** para un lector de asistencia) en `READER_API_KEY` de
`include/secrets.h`, recompila y reflashea. `./run setup` re-imprime las
**mismas** claves de las filas existentes — nunca las rota.

**Opción B — fija tu propia clave en la fila del lector (cuando ya
escribiste una clave concreta en secrets.h).** En el host del backend:

```bash
php artisan tinker
>>> App\Models\Reader::where('label', 'Demo Reader — Classroom/PAE')
...     ->first()->update(['api_key' => 'TU-CLAVE-DE-secrets.h']);
```

**Opción C — provisionamiento de producción.** Las filas de lectores (y
sus claves) se crean del lado del servidor por la herramienta de admin del
colegio — el firmware nunca participa en eso.

**Verifica la clave ANTES de tocar el firmware** (desde cualquier máquina
que alcance el backend). La comprobación necesita un `credential_uid`
sembrado **real** — con un UID inventado no puedes distinguir de un vistazo
"clave rota" de "UID desconocido" (ver la tabla de abajo):

```bash
curl -i -X POST http://<backend>/api/v1/events/tap \
     -H "Authorization: Bearer <READER_API_KEY>" \
     -H "Content-Type: application/json" \
     -d '{"credential_uid": "PEGA-AQUI-UN-credential_uid-REAL"}'
```

**Dónde conseguir un `credential_uid` real** (elige uno):

- La **tabla de tarjetas** de tu salida de `./run setup` — la tabla
  impresa justo **encima** de la de lectores (re-ejecuta `./run setup`
  cuando quieras: re-imprime las MISMAS tarjetas, nunca las regenera).
- En el host del backend:

  ```bash
  php artisan tinker
  >>> App\Models\Card::where('status', 'active')->pluck('credential_uid', 'id')
  ```

- Cualquier tarjeta que **tú** hayas emparejado con el flujo de
  EMPAREJAR: su UID es el que el dispositivo imprimió como
  `[NFC] card / tarjeta: <uid>` al tocarla.

Cómo leer la respuesta — cada código HTTP tiene exactamente un significado:

| HTTP | Significado para ESTA comprobación |
|---|---|
| `401` | La clave **no está provisionada** — ninguna fila de `readers` coincide con ella. Corrígelo aquí (Opción A/B de arriba) antes de tocar el firmware. |
| `404 Card not recognized` | **La comprobación de clave PASÓ.** El backend ya aceptó la identidad del lector (de lo contrario habría devuelto 401); solo falló la búsqueda de la tarjeta — el UID es inventado, mal tecleado o sin emparejar. Tu clave está bien; usa un `credential_uid` real para ver el flujo completo. |
| `200` | La clave **y** esa tarjeta están vivas — el flujo de toque funciona de extremo a extremo. |

### 3. Lado del firmware

- `include/secrets.h` completo: `WIFI_SSID`, `WIFI_PASSWORD`,
  `API_BASE_URL` (sin barra final), `READER_API_KEY`, `MODE_PASSWORD`.
- Flasheado y monitorizando: `pio run -e esp32dev -t upload && pio device
  monitor`. El banner de arranque termina en modo OPERATION.

### 4. Un token de admin para armar

Armar exige `auth:sanctum` + `role:admin` — una **sesión** de admin
(panel) o un **token de acceso personal** (PAT). Para acuñar un PAT en el
host del backend:

```bash
php artisan tinker
>>> App\Models\User::where('email', 'admin@presence.test')
...     ->first()->createToken('pairing-arm')->plainTextToken;
```

(Los admin pasan sin `403`; un token de teacher se rechaza con `403`, los
invitados con `401` — los roles se aplican en el servidor.) Para encontrar
al estudiante: el panel de admin, o
`App\Models\Student::pluck('name', 'id')` en tinker.

---

## Recorrido — la ruta feliz completa

Paso a paso, con las líneas exactas que imprime el firmware (los builds de
TASK-004 añaden líneas de guía tras cada estado; builds anteriores solo
imprimen la primera línea):

1. **Arma para el estudiante** (host del backend, dentro de los 45 s
   previos al toque):

   ```bash
   curl -X POST http://192.168.1.6:8000/api/v1/admin/students/3/arm-pairing \
        -H "Authorization: Bearer <PAT-de-admin>" \
        -H "Accept: application/json"
   # → {"status":"ok","student_id":3,"expires_at":"2026-09-05T14:03:41.000000Z"}
   ```

2. **Cambia el dispositivo a EMPAREJAR** — escribe `MODE_PASSWORD` + Enter
   en el Monitor Serial (los caracteres se muestran como `*`):

   ```text
   ********
   [MODE] switched to / cambiado a: PAIRING / EMPAREJAR
   [MODE] arm a session first (admin, 45 s window), then tap a FRESH card —
        docs/PAIRING.md / arma primero una sesion (admin, ventana de 45 s),
        luego acerca una tarjeta NUEVA
   ```

   El LED de MODO pasa al patrón de reposo de emparejamiento (dos
   parpadeos cada ~2 s); el LED de EVENTO hace dos parpadeos lentos de
   confirmación.

3. **Acerca una tarjeta NUEVA** (nunca emparejada antes — p. ej. una
   MIFARE en blanco de la hoja) dentro de la ventana:

   ```text
   [NFC] card / tarjeta: 62041607
   [OK] card paired to / tarjeta emparejada con: Maria González
   ```

   LED de EVENTO sólido ~1.5 s. En el backend: existe una nueva fila en
   `cards` (`credential_uid` 62041607 → estudiante 3), el emparejamiento
   pendiente queda consumido y el lector que lo consumió queda registrado.

4. **Úsala de inmediato** — vuelve al modo OPERATION (`MODE_PASSWORD` +
   Enter) y acerca la misma tarjeta:

   ```text
   [MODE] switched to / cambiado a: OPERATION / OPERACION
   [MODE] tap a PAIRED card to log the event / acerca una tarjeta EMPAREJADA
        para registrar el evento
   ...
   [NFC] card / tarjeta: 62041607
   [OK] event logged / evento registrado — Maria (CLASS_ATTENDANCE)
   ```

   Las tarjetas recién emparejadas nacen activas: el toque funciona de
   inmediato, sin pasos extra.

5. **Empareja al siguiente estudiante** — arma de nuevo (cada sesión es de
   un solo uso) y repite. Armar con una sesión previa aún abierta
   simplemente la superpone (gana la más reciente).

---

## Qué significa cada resultado

`parsePairResponse` decide por el código HTTP; el `message` del backend se
imprime solo para el log (localizado en el servidor, EN por defecto — el
firmware nunca decide por texto del mensaje).

| Línea serial | HTTP | LED | Significado en el backend | Causa → arreglo |
|---|---|---|---|---|
| `[OK] card paired to / tarjeta emparejada con: <nombre>` | 200 | sólido 1.5 s | Tarjeta creada y vinculada; sesión consumida | — |
| `[401] reader key rejected ...` + líneas de remediación | 401 | 6 parpadeos rápidos | Ninguna fila de `readers` coincide con la clave Bearer | Clave sin provisionar / error de tipeo → [Prerrequisitos §2](#2-reader_api_key-registrada-en-el-backend--la-lista-de-chequeo-del-401) |
| `[409] <mensaje>` + líneas de armar-primero | 409 | 3 parpadeos | Sin sesión activa (nadie armó, expiró tras 45 s, o ya fue consumida) | Arma **antes** de tocar, y re-arma tras cada éxito → [TL;DR](#tldr--el-flujo-de-un-vistazo) |
| `[422] <mensaje>` + líneas de tarjeta-nueva | 422 | 4 parpadeos | Ese UID ya tiene fila en `cards` | Usa una tarjeta nueva — la sesión **sigue armada**, reintenta de inmediato |
| `[NET] network failure / fallo de red` | transporte | 5 parpadeos rápidos | Wi-Fi/backend inalcanzable (DNS, conexión rechazada, timeout) | Revisa AP / backend; el dispositivo se autorecupera, sin reiniciar |
| `[ERR] unexpected / inesperado: <msg>` | 5xx / otro | sólido largo | Error del servidor o cuerpo malformado | Revisa los logs del backend; el dispositivo sigue responsivo |

---

## Patrones de LED + zumbador (modo de emparejamiento)

| Patrón (LED de EVENTO) | Significado |
|---|---|
| 2 parpadeos lentos (500 ms) | Cambio de modo aceptado (`ModeSwitched`) |
| 2 parpadeos muy rápidos (80 ms) | Cambio rechazado — clave incorrecta (`ModeRejected`) |
| sólido ~1.5 s | Tarjeta emparejada con éxito |
| 3 parpadeos | 409 — no hay sesión de emparejamiento activa |
| 4 parpadeos | 422 — tarjeta ya emparejada (la sesión sigue armada) |
| 6 parpadeos rápidos | 401 — clave de lector rechazada |
| 5 parpadeos rápidos | Fallo de red |
| sólido largo | Error del servidor / desconocido |

El **LED de MODO** sigue siendo la fuente de verdad siempre visible del
modo actual: reposo de emparejamiento = **dos** parpadeos cortos cada ~2 s
(reposo de operación = uno).

---

## Solución de problemas

**`[401]` en cada toque/emparejamiento** — la clave no está provisionada
del lado del servidor. Trabaja el [Prerrequisito §2](#2-reader_api_key-registrada-en-el-backend--la-lista-de-chequeo-del-401):
corre el curl de verificación; si da 401, corrige la clave (Opción A o B)
antes de reflashear. Confirma también que no haya espacios ni comillas
extra en la línea `#define READER_API_KEY "..."` — el valor enviado es la
cadena exacta entre comillas.

**`[409]` inmediatamente después de armar** — revisa el orden (armar →
tocar dentro de 45 s), que el armado haya devuelto `{"status":"ok"}` para
el estudiante correcto, y que el toque de nadie más haya consumido la
sesión (cada una es de un solo uso). Si el backend define un
`PAIRING_WINDOW_SECONDS` menor que 45, la ventana real es ese valor (el
texto de guía del dispositivo solo refleja el valor por defecto).

**`[422]` con una tarjeta flamante** — el UID no es realmente nuevo: fue
emparejado antes (este banco, el seeder u otro lector). Los UID son
cadenas sensibles a mayúsculas; `62041607` ≠ `62041607 ` (espacio final).
Para empezar de cero con un estudiante, empareja otra tarjeta física; para
revincular el MISMO UID a otro estudiante, borra la fila vieja en `cards`
del lado del servidor (acción explícita de admin — por diseño, nunca desde
el dispositivo).

**`[NET]` en medio del emparejamiento** — la ventana armada sigue corriendo
en el servidor (45 s desde el armado, no desde tu toque). Si la red se
recupera dentro de la ventana, solo acerca la tarjeta de nuevo; si no,
re-arma. El dispositivo reconecta el Wi-Fi en segundo plano por sí solo.

**El toque de una tarjeta recién emparejada da 404 en OPERATION** —
inesperado: las tarjetas nacen activas. Revisa el cuerpo de la respuesta
del emparejamiento (¿fue realmente `200`?) y el `status` de la tarjeta en
el servidor.

---

## Preguntas frecuentes

**El curl de verificación respondió `404 Card not recognized` — ¿mi clave
está rota?** No — es lo contrario. `401` es la respuesta de "clave sin
provisionar". El `404` lo produce el manejador del toque **después** de
que la identidad del lector fue aceptada, así que un `404` demuestra que
la clave funciona; solo falló la búsqueda de la tarjeta (UID inventado o
mal tecleado, o una tarjeta que nadie ha emparejado). Suele significar
que el UID del ejemplo era un marcador y no un `credential_uid` sembrado
real — mira [Prerrequisitos §2](#2-reader_api_key-registrada-en-el-backend--la-lista-de-chequeo-del-401)
para saber dónde viven los reales, y re-ejecuta el curl para ver el `200`.

**¿Puede el lector emparejarse solo con el backend (registro del
dispositivo)?** No — deliberadamente. Un dispositivo que puede
autoautorizarse es un agujero de lector-pirata; las claves nacen del lado
del servidor ([Opciones A/B/C](#prerrequisitos-hazlos-una-vez)).

**¿Quién puede armar un emparejamiento?** Admins (sesión Sanctum o PAT).
Un teacher se rechaza con `403`, un invitado con `401` — se aplica en el
backend, no en este firmware.

**¿Por qué 45 segundos?** Suficiente para caminar del escritorio al lector
y tocar; demasiado corto para dejar invitaciones abiertas. Es el
`PAIRING_WINDOW_SECONDS` del backend (por defecto 45) — el texto de pista
de la consola serial refleja el valor por defecto; el backend es siempre
la autoridad.

**¿La sesión es de un solo uso?** Sí — la consume el primer
emparejamiento exitoso. Armar de nuevo es la forma normal de emparejar al
siguiente estudiante (si se solapan, gana la sesión armada más reciente).

**Se perdió una tarjeta / un estudiante necesita un reemplazo.** Empareja
una tarjeta física NUEVA igual (armar → tocar). La fila de la tarjeta
vieja permanece en el servidor (activa o no, jamás se reasigna en
silencio).

**¿Importan varios lectores?** No — cualquier lector cuya clave acepte el
backend puede completar la sesión armada. El emparejamiento pendiente
registra qué lector la consumió.

**¿El modo de emparejamiento cambia lo que hace el dispositivo en
OPERATION?** No. Los modos solo eligen a qué endpoint va cada toque; la
identidad, el Wi-Fi y la configuración quedan intactos.

**¿Dónde se especifica exactamente el contrato de emparejamiento?** En
[API_INTEGRATION.es.md](API_INTEGRATION.es.md) (tablas de
petición/respuesta) y en B2B-Core `docs/API.es.md` §arm-pairing /
§cards-pair.

---

## Notas de seguridad

- **Armar-antes-de-emparejar es autorización con humano en el circuito**:
  la decisión tarjeta-estudiante es de un admin, ocurre en el backend y
  vale para una tarjeta durante 45 s. El lector es un terminal que teclea
  tarjetas, no una autoridad.
- **La clave del lector es la identidad del dispositivo.** Rótala del lado
  del servidor (`readers.api_key`) si un dispositivo queda comprometido;
  el firmware la guarda solo en el gitignored `secrets.h`.
- **La clave de modo (ADR-005) es una compuerta de operador, no
  criptografía** — evita cambios accidentales de modo por la consola USB.
  No protege nada del lado del servidor: emparejar sigue exigiendo una
  sesión armada.
- **Sin reasignación silenciosa, sin sesiones permanentes, sin identidad
  suministrada por el cliente** — los tres invariantes sobre los que está
  construido todo el flujo.
