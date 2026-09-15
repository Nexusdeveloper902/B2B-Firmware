# Protocolo de credencial HCE de Pulse (v1)

> Also available in: [English](HCE_PROTOCOL.md)
> Referencia canónica a nivel de bytes para la ruta teléfono-como-credencial.
> Lado teléfono: `B2B-App/pulse-credential` (`ApduProtocol`).
> Lado lector: `lib/PresenceCore` (`HceProtocol`) + `lib/NfcReader`
> (rama HCE de `Rc522NfcReader`). Contrato backend: B2B-Core
> `docs/API.es.md` §“Credenciales HCE de Android”. Adaptado del prototipo
> standalone verificado (`RC522-Android/`, `PROTOCOL.md` allí) — los bytes
> del protocolo no cambiaron; lo que cambió es la integración alrededor
> (kind de credencial, endpoints Pulse, UI del escritorio).

Teléfono-como-credencial sobre NFC para ESP32 + RC522. Deliberadamente
mínimo: dos APDUs.

```text
NFC-A
  ↓  anticolisión + SELECT (bit 6 del SAK => soporta T=CL)
ISO-DEP (ISO/IEC 14443-4)
  ↓  RATS / ATS (+ PPS), I-blocks vía MFRC522Extended::TCL_Transceive
APDU
  ↓  SELECT AID -> CHALLENGE -> verificación
Protocolo de credencial Pulse (identidad a nivel de aplicación)
  ↓  credential_uid + credential_kind=hce → B2B-Core
```

Nada de MIFARE Classic en esta ruta: Android HCE no puede emularlo, así
que el teléfono se presenta como objetivo ISO-DEP. Las tarjetas físicas
conservan su propia ruta intacta (UID RF → sin `credential_kind`).

## Capa RF

| Capa | Detalle |
|---|---|
| RF | NFC-A, 106 kbit/s (PPS negocia ≤ 212 kbit/s solo si el teléfono lo ofrece) |
| Anticolisión | SELECT en cascada estándar; SAK `0x20` = objetivo ISO-DEP (HCE) |
| Activación | RATS (`E0 50` + CRC_A, FSD = 64, CID = 0) → ATS; PPS solo si ATS/TA1 presente |
| Tramas | I-blocks ISO-DEP, CID = 0, sin NAD; el lector confirma el encadenado teléfono→lector |
| Límites | FIFO del RC522 = 64 B, así un I-block lleva ≤ ~57 B de APDU |
| Driver | `MFRC522Extended` (misma librería `miguelbalboa/MFRC522`, sin dependencia extra): el override de `PICC_Select()` hace anticolisión + SELECT + RATS/ATS + PPS cuando el bit 6 del SAK señala T=CL; `TCL_Transceive(tag, apdu, len, resp, &respLen)` hace I-blocks con alternancia de número de bloque, manejo de CID y confirmación de respuestas encadenadas. Límites conocidos (documentados, no remediados): FSD fijo en 64, CID fijo en 0, sin NAD, sin WTX por S-block (el teléfono responde en ms — HCE lo hace), bitrates sobre 212 kbit/s no intentados. |

## Capa de aplicación

- AID de Pulse: **`F0010203040506`** (7 bytes, categoría propietaria
  `F…` — válido, consistente en ambos lados, no colisiona con nada
  productivo).
- Id de credencial: ASCII de longitud variable (prototipo:
  `TEST-ANDROID-001`, 16 bytes, público, no secreto). El lector acepta
  1–32 bytes.
- **El UID NFC del teléfono es aleatorio por toque en Android HCE** — el
  lector solo registra su longitud (prueba de descubrimiento) y JAMÁS lo
  usa como identidad. El backend jamás lo recibe: no hay columna
  `rf_uid`, no hay entrada `rf_uid`, y la búsqueda del tap es solo por
  `credential_uid`.

## APDU 1 — SELECT AID

Petición (12 bytes, sin `Le`):

```text
CLA | INS | P1 | P2 | Lc | AID (7 bytes)
 00 | A4  | 04 | 00 | 07 | F0 01 02 03 04 05 06
```

o sea `00 A4 04 00 07 F0010203040506`. Un `Le = 00` final se acepta y se
ignora.

Respuesta, AID coincidente:

```text
48 43 45 2D 4F 4B ("HCE-OK") | 90 00
```

Respuesta, AID no coincidente: `6A 82` (app no encontrada). La sesión
pasa a "seleccionada" solo al coincidir; se reinicia al desactivarse RF.

## APDU 2 — CHALLENGE (challenge-response)

Petición (13 bytes, sin `Le`; `Le = 00` final aceptado):

```text
CLA | INS | P1 | P2 | Lc | nonce (8 bytes, esp_random fresco por toque)
 80 | 10  | 00 | 00 | 08 | NN NN NN NN NN NN NN NN
```

Respuesta:

```text
credLen (1) | credId (1-32, ASCII) | HMAC-SHA256 (32) | 90 00
MAC = HMAC-SHA256(key = HCE_SECRET, msg = credId || nonce)
```

El lector recomputa el MAC sobre el id recibido más el nonce enviado y
compara en tiempo constante. Los secretos jamás se registran (solo el
nonce por toque y el id público); un mismatch falla cerrado.

## Integración Pulse (lo que el prototipo no tenía)

| Paso | Comportamiento |
|---|---|
| Lector → backend (emparejar) | `POST /api/v1/admin/cards/pair` con `{"credential_uid": "<credId>", "credential_kind": "hce"}` — mismo flujo arma-then-pair, misma semántica 409/422 que las tarjetas físicas |
| Lector → backend (tap) | `POST /api/v1/events/tap` con `{"credential_uid": "<credId>"}` — el kind NO se envía a propósito (la búsqueda es solo por uid); los toques de teléfono resuelven `credencial → estudiante → asistencia / PAE / reciclaje` por la espina sin cambios |
| Almacenamiento | `cards.kind` (`physical` \| `hce`, por defecto `physical`) — metadato de visualización/auditoría; el firmware viejo que omite el kind empareja igual que antes |
| UI del escritorio | Credenciales de teléfono marcadas (“Phone” / “Teléfono”) en chips del roster, historial reciente (filas SSR + en vivo por WS) y escritorio de estudiantes |
| Provisionamiento de clave | `HCE_SECRET` en `secrets.h` / `secrets.camera.h` gitignorados (las plantillas lo documentan); DEBE coincidir con el secreto de la app Android o todo toque de teléfono falla cerrado. Un secrets anterior a HCE compila con el valor de desarrollo + `#warning` (mismo precedente que `MODE_PASSWORD`) |

## Status words

| SW | Significado | Cuándo |
|---|---|---|
| `9000` | Éxito | SELECT coincidente / CHALLENGE respondido |
| `6A82` | App no encontrada | SELECT con otro AID |
| `6985` | Condiciones no satisfechas | CHALLENGE antes de SELECT |
| `6700` | Longitud errónea | `Lc` ≠ 7 (SELECT) / ≠ 8 (CHALLENGE), APDU truncado |
| `6D00` | INS desconocido | CLA conocido, instrucción desconocida |
| `6E00` | CLA desconocido | Primer byte no `00`/`80` |

## Longitudes máximas

| Ítem | Máx |
|---|---|
| Petición SELECT | 13 bytes (con Le opcional) |
| Respuesta SELECT | 8 bytes |
| Petición CHALLENGE | 14 bytes (con Le opcional) |
| Respuesta CHALLENGE | hasta 67 bytes (1 + 32 + 32 + 2; el prototipo usa 51) |
| INF de un I-block | ~57 B — la respuesta de 51 bytes del prototipo cabe; un id de 32 bytes necesita los ACKs de encadenado del lector (ya implementados en `TCL_Transceive`) |

## Condiciones de error (diagnóstico del lector)

Advertencia del driver (hallada en banco): el `PICC_Select` de MFRC522
1.4.12 parsea un buen ATS en un local de pila y jamás lo guarda en
`tag` — así `tag.ats.size` siempre es 0 por la ruta del driver y
cualquier compuerta sobre él es código muerto (esto también mordió al
prototipo standalone). Dato más profundo: una vez que este teléfono
recibe HaltA no responde nada (RATS, WUPA, ni REQA) hasta re-entrar al
campo — así que el select del driver (RATS inmediato + HaltA al timeout)
envenena el toque, y todo reintento post-halt apunta a un cadáver. Por
tanto park-and-activate: el descubrimiento usa el Select BASE
calificado (sin RATS, sin HaltA sorpresa — tramas idénticas al override
para no-ISO-DEP), un avistamiento ISO-DEP estaciona la sesión intacta
400 ms en silencio de radio, y solo entonces sale el PRIMER RATS del
toque (`adoptAts`, status registrado, ATS adoptado). Un objetivo que no
responda ese RATS cae a la ruta UID. No se negocia PPS — 106 kbit/s es
obligatorio y sobra para intercambios <70 B.

`HCE SELECT failed` (sin
respuesta I-block) · `HCE SELECT rejected (unknown AID)` (`6A82`) ·
`HCE CHALLENGE timeout` · `HCE authentication FAILED (malformed or HMAC
mismatch)`. Cada fallo libera el objetivo (`TCL_Deselect` + `PICC_HaltA`)
y el equipo sigue respondiendo; los toques MIFARE jamás se afectan (esa
ruta no corre para objetivos ISO-DEP, y viceversa).

## Evidencia de independencia del UID

1. **Unitaria** (`test_hce_protocol.cpp::test_hce_identity_ignores_the_rf_uid`):
   los parsers ni siquiera RECIBEN un UID RF — bytes APDU idénticos
   siempre dan la misma credencial.
2. **Backend** (`HceCredentialTest::identification_does_not_depend_on_the_rf_uid`):
   no existe columna `rf_uid`; un id no-hex (imposible como UID RF) se
   empareja y toca de punta a punta, dos veces, al mismo estudiante.
3. **Banco** (checklist §10): registra la longitud del UID RF entre
   toques (cambia), verifica que el mismo id autentica siempre.

## Endurecimiento productivo (explícitamente fuera de alcance)

Clave precompartida única de desarrollo → claves por credencial desde un
backend seguro; añadir protección anti-replay (desafíos rastreados /
contador monotónico); autenticación mutua + canal cifrado (p. ej.
estilo SCP03); rotación de claves; revisión de canal lateral. La clave
Bearer del lector avala la credencial verificada ante el backend — la
misma confianza que un UID físico. Esta especificación prueba la ruta
NFC y la integración Pulse, no un sistema credencial.
