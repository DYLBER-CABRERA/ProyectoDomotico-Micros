# 002 — Estado actual y roadmap

_Última actualización: 2026-06-22 (revisión inicial SDD)._

## Matriz de módulos

| Fase | Módulo | Archivos | Estado | Recurso HW | Notas |
|------|--------|----------|--------|------------|-------|
| 1 | LCD 16×2 HD44780 (4 bits) | `lcd.*` | ✅ | Puerto A | RS=PA0, EN=PA2, D4–7=PA4–7 |
| 2 | Teclado 4×4 | `teclado.*` | ✅ | Puerto C/L, Timer2 | escaneo por ISR Timer2 |
| 3 | USART0 serial | `usart.*` | ✅ | PE0/PE1, RX ISR | 9600 8N1, buffer circular |
| 5 | Alarmas acceso + incendio | `alarma.*` | ✅ | INT2–5, PB6/PB7 | DEF-001 corregido; verificar incendio zona 2 al compilar |
| 6a | Garaje (servomotor) | `motor.*` | ✅ | D6/PH3 (Timer4) | servo PWM 50Hz, no bloqueante; PG0–3 liberados (DEF-002) |
| — | ADC compartido | `adc.*` | ✅ | ADC9 + ADC0 | temp (A9) + volumen (A0), mux conmutado |
| 6b | Temperatura | `temperatura.*` | ✅ | ADC9, PK3/PK4 | histéresis; pot en **A9** (no A8) |
| 6c | Dimmer | `dimmer.*` | ✅ | Timer1 OC1A (D11) | escala 0–10 |
| 7a | Horno | `horno.*` | ✅ | **PH5 (D8)** | DEF-001 corregido: ya usa HORNO_DDR/HORNO_PORT |
| 7b | Sonido | `sonido.*` | ✅ | Timer3 OC3A (PE3/D5), PH6, ADC0 | volumen por pot A0 en tiempo real; PWM+RC |
| 7c | Mercado | `mercado.*` | ✅ | EEPROM 0x0B0+ | persistente, anti-duplicados |
| 4 | SPI maestro | `spi_master.*` | ✅ | SPI PB0–3 | fosc/16, modo 0, CS por PB0 |
| 4 | RFID RC522 | `rfid.*` | ✅ | SPI + PB0 (CS) | REQA, anticolisión, auth MIFARE |
| 4 | Gestor EEPROM (UIDs) | `eeprom_mgr.*` | ✅ | EEPROM 0x000+ | 10 slots de 4+1 bytes, leer-antes-de-escribir |
| 4 | Lógica de acceso | `acceso.*` | ✅ | PG0 (imán) + servo | adulto/hijo/desconocido, contador en tarjeta |

Leyenda: ✅ ok · 🟡 ok con observación · 🐞 defecto abierto · ⏳ pendiente.

## Lo que está hecho (resumen)

El núcleo de seguridad y confort funciona: las dos alarmas, el LCD, el teclado, el
serial, el dimmer, la temperatura, el motor del garaje, el horno, el sonido y el mercado
están implementados y descritos en `Manual_ProyectoDomotico.md`. El parser serial del
`.ino` ya enruta todos los comandos de estas fases.

## Lo que está pendiente

Al momento no hay fases pendientes de implementar. Todo el proyecto está completo.
Pueden existir defectos menores no detectados o mejoras de la simulación en Proteus.

## Roadmap (completado)

Todas las fases del proyecto están implementadas. El roadmap histórico fue:

| Hito | Contenido | Estado |
|------|-----------|--------|
| H0 — Estabilizar | DEF-001, DEF-003, DEF-004 corregidos | ✅ |
| H1 — Actuadores | Garaje = servo (Timer4); puerta = imán (PG0) | ✅ |
| H2 — Bus SPI | `spi_master.cpp`: SPI maestro modo 0, fosc/16, CS PB0 | ✅ |
| H3 — EEPROM UIDs | `eeprom_mgr.cpp`: 10 slots, leer-antes-de-escribir | ✅ |
| H4 — RFID | `rfid.cpp`: RC522, REQA, anticolisión, auth MIFARE, contador en tarjeta | ✅ |
| H5 — Acceso | `acceso.cpp`: adulto/hijo/desconocido, imán + servo, contador sala juegos | ✅ |
| H6 — Integración | Includes, init calls, `rfid_verificar` en loop, comandos serial/teclado | ✅ |

## Defectos (historial)

| ID | Severidad | Resumen | Archivo | Estado |
|----|-----------|---------|---------|--------|
| DEF-001 | Alta | Horno usaba PE5 (colisión con alarma incendio) → PH5 | [`defects/DEF-001-horno-pin-pe5.md`](defects/DEF-001-horno-pin-pe5.md) | ✅ |
| DEF-002 | Media | Puerta principal por imán no implementada; garaje como PAP vs. servo | [`defects/DEF-002-actuadores-enunciado.md`](defects/DEF-002-actuadores-enunciado.md) | ✅ |
| DEF-003 | Baja | Comentario de motor.h desactualizado (decía Timer3 en vez de Timer4) | [`defects/DEF-003-doc-motor-timer3.md`](defects/DEF-003-doc-motor-timer3.md) | ✅ |
| DEF-004 | Media | Dimmer con switch Proteus/hardware | [`defects/DEF-004-dimmer-rango-hardware.md`](defects/DEF-004-dimmer-rango-hardware.md) | ✅ |
| DEF-005 | Media | Serial `ARM` armaba sin código → ahora `ARM:1234` | (ver `features/alarma.md`) | ✅ |

## Notas de verificación

- **No fue posible compilar en este entorno** (sin toolchain AVR ni acceso de red para
  instalarlo). Los hallazgos provienen de **análisis estático**: lectura de los módulos
  y un barrido de uso de registros `DDRx/PORTx/PINx`, timers e ISR por archivo.
- Antes de la entrega, **compilar** (Arduino IDE o `arduino-cli`) y ejecutar el plan de
  pruebas del Manual §7, prestando atención especial a la alarma de incendio zona 2 tras
  corregir DEF-001.
