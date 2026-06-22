# 001 — Requisitos y trazabilidad

Requisitos derivados del enunciado oficial ([`Proyecto_2026-1.md`](../Proyecto_2026-1.md)).
Cada requisito tiene un ID `RF-xx` (funcional) y su estado/ubicación en el código.

## Requisitos funcionales

| ID | Requisito | Estado | Implementación |
|----|-----------|--------|----------------|
| RF-01 | Alarma de **acceso** (puertas y ventanas) mediante sensores | ✅ | `alarma.cpp` (INT2/INT3, SW1/SW2) |
| RF-02 | Alarma de **incendio** con sensores de humo | ✅ | `alarma.cpp` (INT4/INT5, SW3/SW4) |
| RF-03 | Ante una condición de seguridad, **informar por el puerto serial** | ✅ | `ALARMA:ACCESO` / `ALARMA:INCENDIO` vía `usart` |
| RF-04 | Alarmas **activadas/desactivadas solo con código** | ✅ | código "1234" en `alarma.cpp`; teclado A/B (valida código) y serial `ARM:1234`/`DISARM:1234`. *Corregido*: antes `ARM` armaba sin código. **Anti-intrusos**: 3 códigos errados seguidos disparan `ALARMA:INTRUSO`. |
| RF-05 | **Acceso por tarjetas RFID** | ⏳ | `rfid.cpp` (stub) — Fase 4 |
| RF-06 | **Enrolar** nuevas personas autorizadas y **borrar** existentes | ⏳ | `eeprom_mgr.cpp` (stub) — Fase 4 |
| RF-07 | Acceso por **puerta principal (imán)** o **garaje (servomotor)** | 🟡 | garaje con **servomotor** en `motor.cpp` (D6/PH3, Timer4 — DEF-002); puerta principal por imán sin implementar (Fase 4) |
| RF-08 | **Sala de juegos**: cada hijo accede una cantidad de veces programada en su tarjeta; descontar tras cada ingreso | ⏳ | Fase 4 (`rfid_leer/escribir_contador`, `acceso.cpp`) |
| RF-09 | Los **padres cargan** los accesos disponibles de los hijos | ⏳ | Fase 4 (`acceso.cpp` + comandos serial) |
| RF-10 | Control de **iluminación dimerizada** | ✅ | `dimmer.cpp` (Timer1 PWM, niveles 0–10) |
| RF-11 | Control de **temperatura** con calefactor y ventilador | ✅ | `temperatura.cpp` (ADC + histéresis, PK3/PK4) |
| RF-12 | **Horno remoto**: indicar tiempo y temperatura | ✅ | `horno.cpp` (`HORNO:temp,min`) — ver DEF-001 (pin) |
| RF-13 | **Equipo de sonido remoto** con volumen por **señal analógica** proporcional | ✅ | `sonido.cpp`: volumen por **potenciómetro en A13 (ADC13/PK5)**, salida PWM PE3 (Timer3) + filtro RC → señal analógica |
| RF-14 | **Lista de mercado** (nombre + cantidad) consultable remotamente | ✅ | `mercado.cpp` (EEPROM, `MERCADO:ADD/DEL/LIST`) |
| RF-15 | "Toda la información..." (persistencia de datos del sistema) | 🟡 | mercado y código de alarma en EEPROM; UIDs pendientes (Fase 4) |

> RF-15 aparece truncado en el enunciado original; se interpreta como **persistencia en
> EEPROM** de la información de configuración (UIDs, código, mercado). Confirmar con el
> docente el alcance exacto.

## Requisitos no funcionales

| ID | Requisito | Estado |
|----|-----------|--------|
| RNF-01 | Implementación con **registros directos**, sin librerías Arduino | ✅ (constitución Art. 1) |
| RNF-02 | Arquitectura **modular** (un par `.cpp/.h` por subsistema) | ✅ |
| RNF-03 | Debe **simular en Proteus** con el `.hex` compilado | ✅ (ver Manual §5–6) |
| RNF-04 | Reporte/operación por **terminal serial 9600 8N1** | ✅ |
| RNF-05 | Coexistencia de subsistemas sin bloquear seguridad (ISR siempre vivas) | ✅ |

## Brechas frente al enunciado (resumen)

1. **Fase 4 completa pendiente** (RF-05, RF-06, RF-08, RF-09): RFID, enrolamiento/borrado,
   contadores de sala de juegos.
2. **Puerta principal por imán no implementada** (RF-07 → DEF-002). El garaje se mantiene
   con **motor paso a paso** (decisión cerrada; justificar la desviación ante el docente).
3. ✅ **Bug de pin del horno corregido** (RF-12 → DEF-001). Verificar RF-02 (incendio
   zona 2) al compilar.

La planificación para cerrarlas está en `002-estado-y-roadmap.md` y
`fase-04-rfid-acceso/`.
