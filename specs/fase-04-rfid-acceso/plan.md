# Fase 4 — PLAN de implementación

Plan técnico para llevar la Fase 4 de stubs a integrada. Ejecutar en este orden; cada
paso es verificable de forma aislada antes de pasar al siguiente.

## Estrategia

Construir de **abajo hacia arriba** (bus → almacenamiento → driver → lógica → integración)
para poder probar cada capa sin depender de las superiores. Cada módulo respeta el patrón
`*_init()` + API pública del `.h` (la constitución, Art. 2).

## Pasos

### Paso 0 — Decisiones (DEF-002 + contador en tarjeta)
✅ Actuador del garaje decidido: **motor paso a paso** (se reutiliza `motor.cpp`). Falta
cerrar: pin del imán/relé de la puerta principal, y si el contador de la sala de juegos
vive en la tarjeta o en EEPROM. Registrar en `spec.md` §3 y en `003-arquitectura.md`.

### Paso 1 — `spi_master.cpp`
Configurar SPI maestro (modo 0, fosc/16). Implementar `spi_transferir` con espera de
`SPIF`. `SS`/CS como salidas. **Prueba**: leer el registro `VersionReg` del RC522 (debe
devolver `0x91`/`0x92`) o, sin RC522, un loopback MOSI↔MISO.

### Paso 2 — `eeprom_mgr.cpp`
Implementar lectura/escritura por dirección y la gestión de UIDs (`guardar`, `buscar`,
`borrar`) sobre `0x000–0x09F`, con marca de slot vacío. Reusar patrón anti-escritura de
`mercado.cpp`. **Prueba**: guardar 2 UIDs, buscarlos, borrar uno, verificar persistencia
tras reset (dump por serial).

### Paso 3 — `rfid.cpp`
Sobre `spi_master`: reset suave del RC522, encender antena, detectar tarjeta (REQA) y
leer UID (anticolisión). Implementar lectura/escritura del bloque del contador si aplica.
**Prueba**: `rfid_hay_tarjeta()` y `rfid_leer_uid()` devuelven el UID correcto por serial
al acercar una tarjeta.

### Paso 4 — `acceso.cpp`
Implementar los actuadores (imán + garaje según Paso 0) y la lógica adulto/hijo/sala de
juegos usando `eeprom_mgr` y `rfid`. **Prueba**: simular UID conocido/desconocido y
verificar concesión/denegación y descuento de cupos.

### Paso 5 — Integración en el `.ino`
Descomentar includes, añadir `*_init()` en `setup()`, `rfid_verificar()` en `loop()`, y
los comandos serial `ENROL/BORRAR/ACCESOS`. **Prueba**: flujo completo de extremo a
extremo en Proteus + actualizar Manual §7 con las pruebas nuevas.

### Paso 6 — Cierre
Actualizar `001-requisitos.md` (RF-05/06/08/09 → ✅), `002-estado-y-roadmap.md` y las
specs de `features/` nuevas (`features/rfid.md`, `features/acceso.md`). Cerrar DEF-002.

## Riesgos

| Riesgo | Mitigación |
|--------|-----------|
| RC522 no disponible/realista en Proteus | Acordar simulación alterna del UID con el docente (Paso 0). |
| Colisión SPI (PB) con dimmer/alarma (PB5–7) | SPI usa PB0–3, libres; validar en `003-arquitectura.md`. |
| Escrituras EEPROM excesivas (desgaste) | Leer-antes-de-escribir; escribir contador solo al cambiar. |
| Bloqueo del `loop()` por SPI/RFID | Mantener `rfid_verificar()` no bloqueante; sin esperas largas. |
| (Garaje servo descartado) | Se usa el paso a paso existente; no se requiere timer adicional. |
