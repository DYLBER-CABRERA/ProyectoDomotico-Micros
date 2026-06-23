# Fase 4 — PLAN de implementación

Plan técnico para llevar la Fase 4 de stubs a integrada. Ejecutar en este orden; cada
paso es verificable de forma aislada antes de pasar al siguiente.

## ✅ Fase 4 completada (2026-06-23)

La implementación siguió la estrategia de **abajo hacia arriba**:

1. **SPI**: `spi_master.cpp` — maestro modo 0, fosc/16, CS en PB0.
2. **EEPROM**: `eeprom_mgr.cpp` — 10 slots UID+tipo, leer-antes-de-escribir.
3. **RFID**: `rfid.cpp` — RC522, REQA, anticolisión, auth MIFARE, contador en tarjeta,
   máquina de estados no bloqueante, operaciones pendientes (enrol/borrar/recarga).
4. **Acceso**: `acceso.cpp` — imán en PG0, servo garaje, lógica adulto/hijo.
5. **Integración**: en el `.ino` — includes, inits, `rfid_verificar()` en loop, comandos
   serial y teclado (6x).
6. **Cierre**: specs de features, actualización de estado, cierre de DEF-002.

## Pendiente (verificación en Proteus)

- T1.4: Verificar lectura de `VersionReg` del RC522.
- T2.5: Verificar persistencia de UIDs en EEPROM tras reset.
- T3.5: Verificar lectura de UID por serial al acercar tarjeta.
- T4.4: Verificar flujo adulto/hijo/desconocido.
- T5.5: Verificar flujo completo extremo a extremo.
- T6.4: Añadir pruebas 10–12 al Manual §7.

## Riesgos

| Riesgo | Mitigación |
|--------|-----------|
| RC522 no disponible/realista en Proteus | Acordar simulación alterna del UID con el docente (Paso 0). |
| Colisión SPI (PB) con dimmer/alarma (PB5–7) | SPI usa PB0–3, libres; validar en `003-arquitectura.md`. |
| Escrituras EEPROM excesivas (desgaste) | Leer-antes-de-escribir; escribir contador solo al cambiar. |
| Bloqueo del `loop()` por SPI/RFID | Mantener `rfid_verificar()` no bloqueante; sin esperas largas. |
| (Garaje servo descartado) | Se usa el paso a paso existente; no se requiere timer adicional. |
