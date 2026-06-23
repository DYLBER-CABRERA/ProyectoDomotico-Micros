# Fase 4 — TASKS (desglose atómico)

Tareas accionables para un agente. Marcar `[x]` al completar. Cada tarea indica el
archivo y su criterio de "hecho". Respetar el orden (dependencias entre pasos).

## Paso 0 — Decisiones
- [x] **T0.1** Actuador del garaje → **servomotor** (`motor.cpp`, Timer4). Decidido 2026-06-22 (DEF-002).
- [x] **T0.2** Pin del imán/relé de la puerta principal → **PG0 (D41)**. Salida digital.
- [x] **T0.3** Contador de sala de juegos → **en la tarjeta RFID** (bloque 4 MIFARE).

## Paso 1 — SPI (`spi_master.cpp`)
- [x] **T1.1** Implementar `spi_init()`: `SS,SCK,MOSI` como salida, `MISO` entrada; `SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR0)` (fosc/16, modo 0).
- [x] **T1.2** Implementar `spi_transferir(dato)`: cargar `SPDR`, esperar `SPSR & (1<<SPIF)`, retornar `SPDR`.
- [x] **T1.3** Implementar `spi_cs_bajar()`/`spi_cs_subir()` con el pin CS del RC522 (PB0).
- [ ] **T1.4** Pendiente: verificar en Proteus (lectura de `VersionReg` del RC522 ≈ `0x9x`).

## Paso 2 — EEPROM UIDs (`eeprom_mgr.cpp`)
- [x] **T2.1** `eeprom_init()` + `eeprom_escribir`/`eeprom_leer` con leer-antes-de-escribir.
- [x] **T2.2** `eeprom_guardar_uid(uid,tipo)`: primer slot libre en `0x000–0x09F`.
- [x] **T2.3** `eeprom_buscar_uid(uid)`: índice o `0xFF`.
- [x] **T2.4** `eeprom_borrar_uid(uid)`: marca el slot como vacío (0xFF).
- [ ] **T2.5** Pendiente: verificar persistencia en Proteus.

## Paso 3 — RFID (`rfid.cpp`)
- [x] **T3.1** `rfid_init()`: reset suave RC522 + antena ON (sobre `spi_master`).
- [x] **T3.2** `rfid_hay_tarjeta()` (REQA 7 bits) y `rfid_leer_uid(uid)` (anticolisión CL1).
- [x] **T3.3** `rfid_leer_contador`/`rfid_escribir_contador` con auth MIFARE (key A 0xFF×6).
- [x] **T3.4** `rfid_verificar()` no bloqueante para `loop()` (máquina de 3 estados + op. pendientes).
- [ ] **T3.5** Pendiente: verificar UID por serial en Proteus.

## Paso 4 — Acceso (`acceso.cpp`)
- [x] **T4.1** `acceso_init()` configura PG0 como salida (imán).
- [x] **T4.2** `acceso_abrir_principal`/`cerrar_principal` (imán PG0) y `acceso_abrir_garaje`/`cerrar_garaje` (servo).
- [x] **T4.3** Lógica adulto/hijo/desconocido + descuento de cupos (`rfid.cpp`).
- [ ] **T4.4** Pendiente: verificar en Proteus.

## Paso 5 — Integración (`ProyectoDomotico.ino`)
- [x] **T5.1** Descomentar `#include` de Fase 4.
- [x] **T5.2** Añadir `spi_init(); eeprom_init(); rfid_init(); acceso_init();` en `setup()`.
- [x] **T5.3** Añadir `rfid_verificar();` en `loop()` (tras alarmas, antes de teclado).
- [x] **T5.4** Añadir comandos `ENROL:ADULTO`, `ENROL:HIJO,n`, `BORRAR`, `ACCESOS:n` al parser.
- [ ] **T5.5** Pendiente: verificar flujo completo en Proteus.

## Paso 6 — Cierre documental
- [x] **T6.1** Crear `features/rfid.md`, `features/acceso.md`, `features/spi_master.md`, `features/eeprom_mgr.md`.
- [x] **T6.2** Actualizar `001-requisitos.md`, `002-estado-y-roadmap.md`, `003-arquitectura.md`, `AGENTS.md`.
- [x] **T6.3** Cerrar DEF-002.
- [ ] **T6.4** Pendiente: añadir pruebas 10–12 (RFID/acceso/sala de juegos) al Manual §7.
