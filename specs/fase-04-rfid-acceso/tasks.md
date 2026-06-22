# Fase 4 — TASKS (desglose atómico)

Tareas accionables para un agente. Marcar `[x]` al completar. Cada tarea indica el
archivo y su criterio de "hecho". Respetar el orden (dependencias entre pasos).

## Paso 0 — Decisiones
- [x] **T0.1** Actuador del garaje → **motor paso a paso** (`motor.cpp`). Decidido 2026-06-22 (DEF-002).
- [ ] **T0.2** Confirmar pin del imán/relé de la puerta principal (salida digital libre).
- [ ] **T0.3** Confirmar si el contador de sala de juegos vive en la tarjeta o en EEPROM.

## Paso 1 — SPI (`spi_master.cpp`)
- [ ] **T1.1** Implementar `spi_init()`: `SS,SCK,MOSI` como salida, `MISO` entrada; `SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR0)` (fosc/16, modo 0).
- [ ] **T1.2** Implementar `spi_transferir(dato)`: cargar `SPDR`, esperar `SPSR & (1<<SPIF)`, retornar `SPDR`.
- [ ] **T1.3** Implementar `spi_cs_bajar()`/`spi_cs_subir()` con el pin CS del RC522.
- [ ] **T1.4** Verificar: lectura de `VersionReg` del RC522 ≈ `0x9x` (o loopback) impresa por serial.

## Paso 2 — EEPROM UIDs (`eeprom_mgr.cpp`)
- [ ] **T2.1** `eeprom_init()` (no-op o marca de zona inicializada) + `eeprom_escribir`/`eeprom_leer` con leer-antes-de-escribir.
- [ ] **T2.2** `eeprom_guardar_uid(uid,tipo)`: primer slot libre en `0x000–0x09F`; retorna índice o error si llena.
- [ ] **T2.3** `eeprom_buscar_uid(uid)`: índice o `0xFF`.
- [ ] **T2.4** `eeprom_borrar_uid(uid)`: marca el slot como vacío.
- [ ] **T2.5** Verificar: guardar/buscar/borrar 2 UIDs; persistencia tras reset (dump serial).

## Paso 3 — RFID (`rfid.cpp`)
- [ ] **T3.1** `rfid_init()`: reset suave RC522 + antena ON (sobre `spi_master`).
- [ ] **T3.2** `rfid_hay_tarjeta()` (REQA) y `rfid_leer_uid(uid)` (anticolisión).
- [ ] **T3.3** `rfid_leer_contador`/`rfid_escribir_contador` (si contador en tarjeta) — si no, devolver desde EEPROM.
- [ ] **T3.4** `rfid_verificar()` no bloqueante para `loop()`.
- [ ] **T3.5** Verificar: UID correcto por serial al acercar tarjeta.

## Paso 4 — Acceso (`acceso.cpp`)
- [ ] **T4.1** `acceso_init()` configura los pines de imán/relé (y servo si Opción A).
- [ ] **T4.2** `acceso_abrir_principal`/`cerrar_principal` (imán) y `acceso_abrir_garaje`/`cerrar_garaje` (actuador acordado).
- [ ] **T4.3** Lógica adulto/hijo/desconocido + descuento de cupos de sala de juegos.
- [ ] **T4.4** Verificar: UID adulto abre; hijo con cupos descuenta; hijo sin cupos deniega; desconocido deniega.

## Paso 5 — Integración (`ProyectoDomotico.ino`)
- [ ] **T5.1** Descomentar `#include` de `spi_master.h`, `rfid.h`, `eeprom_mgr.h`, `acceso.h`.
- [ ] **T5.2** Añadir `spi_init(); eeprom_init(); rfid_init(); acceso_init();` en `setup()` antes de `sei()`.
- [ ] **T5.3** Añadir `rfid_verificar();` en `loop()` (tras alarmas).
- [ ] **T5.4** Añadir comandos `ENROL:ADULTO`, `ENROL:HIJO,n`, `BORRAR`, `ACCESOS:uid,n` al parser.
- [ ] **T5.5** Verificar: flujo completo en Proteus.

## Paso 6 — Cierre documental
- [ ] **T6.1** Crear `features/rfid.md` y `features/acceso.md` con el comportamiento real.
- [ ] **T6.2** Actualizar `001-requisitos.md` (RF-05/06/08/09 → ✅) y `002-estado-y-roadmap.md`.
- [ ] **T6.3** Cerrar DEF-002 (y verificar DEF-001 ya resuelto antes de empezar Fase 4).
- [ ] **T6.4** Añadir pruebas 10–12 (RFID/acceso/sala de juegos) al Manual §7.
