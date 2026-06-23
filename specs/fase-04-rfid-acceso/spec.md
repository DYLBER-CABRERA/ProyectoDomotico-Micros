# Fase 4 — RFID y control de acceso · SPEC

> Cubre RF-05, RF-06, RF-08, RF-09 y parte de RF-07/RF-15. Es el bloque pendiente más
> grande del proyecto. Hoy `spi_master.cpp`, `rfid.cpp`, `eeprom_mgr.cpp` y `acceso.cpp`
> son stubs vacíos y sus `#include` están comentados en el `.ino`.

## 1. Objetivo

Permitir el ingreso a la vivienda mediante **tarjetas RFID (módulo RC522 por SPI)**,
con enrolamiento y borrado de personas autorizadas, distinción **adulto / hijo**, apertura
de **puerta principal (imán)** y **garaje**, y un **contador de accesos a la sala de
juegos** por hijo que se descuenta en cada ingreso y que los padres pueden recargar.

## 2. Requisitos cubiertos

| ID | Detalle |
|----|---------|
| RF-05 | Leer UID de tarjetas RC522 y conceder/denegar acceso. |
| RF-06 | Enrolar nuevas tarjetas (adulto/hijo) y borrar existentes. |
| RF-07 | Abrir puerta principal (imán) y garaje según el punto de acceso. |
| RF-08 | Cada hijo tiene N accesos a la sala de juegos; descontar tras cada ingreso; al llegar a 0, denegar. |
| RF-09 | Los padres (tarjeta adulto o comando serial) recargan los accesos de un hijo. |
| RF-15 | Persistir UIDs/tipos/contadores en EEPROM. |

## 3. Decisiones tomadas

1. **Garaje**: servomotor por PWM en Timer4/OC4A/PH3 (`motor.cpp`). No bloqueante.
   Se reutiliza vía `acceso_abrir_garaje()`/`acceso_cerrar_garaje()`. ✅
2. **Puerta principal (imán/relé)**: **PG0 (D41)**, salida digital simple. ✅
3. **Contador de sala de juegos**: **en la tarjeta RFID** (bloque 4 MIFARE, primer byte).
   Lectura/escritura con autenticación key A (0xFF×6). ✅
4. **Hardware RC522 en Proteus**: pendiente de verificar. Si no hay modelo, simular
   lectura de UID vía entrada serial acordada con el docente.

## 4. Diseño por módulo

### 4.1 `spi_master` (bus SPI maestro)
- SPI por hardware del Mega en **Puerto B**: `SS=PB0(D53)`, `SCK=PB1(D52)`,
  `MOSI=PB2(D51)`, `MISO=PB3(D50)`.
- API (ya declarada en `spi_master.h`): `spi_init()`, `spi_transferir(byte)`,
  `spi_cs_bajar()`, `spi_cs_subir()`.
- Configuración: maestro, modo 0 (CPOL=0, CPHA=0), reloj ≤ 4 MHz (RC522 admite hasta
  10 MHz; usar fosc/16 = 1 MHz para margen). `SS` debe quedar como salida aunque se use
  CS por software (evita que el SPI pase a esclavo).

### 4.2 `eeprom_mgr` (UIDs autorizados)
- API (ya declarada en `eeprom_mgr.h`): `eeprom_init`, `eeprom_escribir`, `eeprom_leer`,
  `eeprom_buscar_uid` (índice o `0xFF`), `eeprom_guardar_uid(uid,tipo)`,
  `eeprom_borrar_uid`.
- Mapa: `0x000–0x09F`, 10 registros de 5 bytes (4 UID + 1 tipo: 0=adulto,1=hijo). Marca
  de slot vacío a definir (p. ej. primer byte `0xFF`).
- Reutilizar el patrón anti-escritura de `mercado.cpp` (leer antes de escribir).
- **No solapar** con mercado (`0x0B0+`) ni con el código de alarma (`0x0A0–0x0A3`).

### 4.3 `rfid` (RC522)
- API (ya declarada en `rfid.h`): `rfid_init`, `rfid_hay_tarjeta`, `rfid_leer_uid(uid)`,
  `rfid_leer_contador(uid,*valor)`, `rfid_escribir_contador(uid,valor)`, `rfid_verificar`.
- Sobre `spi_master`: secuencia mínima del RC522 (reset suave, antena ON, REQA/anticolisión
  para UID). El contador de sala de juegos se guarda/lee en un bloque de datos de la
  tarjeta (si se eligió "en la tarjeta").
- `rfid_verificar()` se llama desde `loop()` (no bloqueante: si no hay tarjeta, retorna).

### 4.4 `acceso` (lógica de alto nivel)
- API (ya declarada en `acceso.h`): `acceso_init`, `acceso_abrir_principal`,
  `acceso_cerrar_principal`, `acceso_abrir_garaje`, `acceso_cerrar_garaje`.
- Flujo: tarjeta presente → leer UID → buscar en EEPROM.
  - **Adulto**: acceso concedido; abrir el actuador del punto de acceso.
  - **Hijo**: si va a la sala de juegos, leer contador; si > 0, conceder y **descontar 1**;
    si 0, denegar (`ACCESO:DENEGADO_SIN_CUPOS`).
  - **Desconocido**: denegar (`ACCESO:DENEGADO`).
- Garaje: reutiliza el **motor paso a paso** (`motor.cpp`: `garaje_abrir`/`garaje_cerrar`),
  decisión cerrada en DEF-002.
- Puerta principal: activa el imán/relé (pin digital libre, a definir en T0.2).

## 5. Protocolo serial añadido (propuesta — confirmar formato)

| Comando | Acción | OK | ERROR |
|---------|--------|----|----|
| `ENROL:ADULTO,<cod>` | Enrolar adulto (requiere código alarma) | `OK:ENROLANDO_ADULTO` → `OK:ENROLADO_ADULTO` | `ERROR:CODIGO` / `ERROR:EEPROM_LLENA_O_DUPLICADO` |
| `ENROL:HIJO,<n>,<cod>` | Enrolar hijo con `n` cupos | `OK:ENROLANDO_HIJO,N` → `OK:ENROLADO_HIJO,N` | idem |
| `BORRAR,<cod>` | Borrar próxima tarjeta | `OK:BORRANDO` → `OK:BORRADO` | `ERROR:CODIGO` / `ERROR:UID_NO_EXISTE` |
| `ACCESOS:<n>,<cod>` | Recargar `n` accesos | `OK:RECARGANDO,N` → `OK:ACCESOS,N` | `ERROR:CODIGO` / `ERROR:UID_NO_EXISTE` / `ERROR:UID_NO_ES_HIJO` |

Alertas asíncronas: `ACCESO:CONCEDIDO,<uid>`, `ACCESO:DENEGADO`,
`ACCESO:DENEGADO_SIN_CUPOS`, `SALA_JUEGOS:<uid>,<restantes>`.

## 6. Integración en `ProyectoDomotico.ino`

1. Descomentar los `#include` de Fase 4.
2. En `setup()`, antes de `sei()`: `spi_init(); eeprom_init(); rfid_init(); acceso_init();`.
3. En `loop()`, en la sección de prioridad adecuada (tras alarmas): `rfid_verificar();`.
4. Añadir los comandos serial nuevos al parser `procesar_comando_serial()`.

## 7. Criterios de aceptación

1. Presentar una tarjeta **adulto** enrolada abre el acceso correspondiente.
2. Presentar una tarjeta **hijo** con cupos descuenta 1 y concede; con 0 cupos deniega.
3. `ENROL:*` y `BORRAR` modifican la EEPROM y persisten tras reset.
4. Los padres recargan cupos (`ACCESOS:` o tarjeta adulto) y el hijo vuelve a entrar.
5. La puerta principal (imán) y el garaje actúan según el punto de acceso (DEF-002).
6. No hay colisión de pines/timers (validado contra `003-arquitectura.md`).
7. Pasa las pruebas manuales nuevas en Proteus (a añadir al Manual §7).
