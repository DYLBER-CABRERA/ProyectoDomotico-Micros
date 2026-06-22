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
| 4 | SPI maestro | `spi_master.*` | ⏳ | SPI (PB0–3) | **stub vacío** |
| 4 | RFID RC522 | `rfid.*` | ⏳ | SPI + CS/RST | **stub vacío** |
| 4 | Gestor EEPROM (UIDs) | `eeprom_mgr.*` | ⏳ | EEPROM 0x000+ | **stub vacío** |
| 4 | Lógica de acceso | `acceso.*` | ⏳ | imán + garaje | **stub vacío** |

Leyenda: ✅ ok · 🟡 ok con observación · 🐞 defecto abierto · ⏳ pendiente.

## Lo que está hecho (resumen)

El núcleo de seguridad y confort funciona: las dos alarmas, el LCD, el teclado, el
serial, el dimmer, la temperatura, el motor del garaje, el horno, el sonido y el mercado
están implementados y descritos en `Manual_ProyectoDomotico.md`. El parser serial del
`.ino` ya enruta todos los comandos de estas fases.

## Lo que falta (resumen)

1. **Fase 4 — RFID y acceso** (el bloque grande pendiente): leer tarjetas RC522 por SPI,
   guardar/buscar/borrar UIDs en EEPROM, distinguir adulto/hijo, abrir puerta principal
   (imán) y garaje, y manejar el contador de la sala de juegos por hijo. Detalle en
   `fase-04-rfid-acceso/spec.md`.
2. **Reconciliar el actuador del garaje** (servo vs. paso a paso) y **agregar la puerta
   principal por imán** (DEF-002).
3. **Corregir el pin del horno** (DEF-001) — bloqueante porque hoy rompe la alarma de
   incendio de zona 2.

## Roadmap propuesto (orden recomendado)

| Hito | Contenido | Depende de |
|------|-----------|-----------|
| **H0 — Estabilizar** | ✅ DEF-001 (pin horno → PH5) y DEF-003 (doc motor) corregidos en código. Falta **verificar incendio zona 2 al compilar**. | — |
| **H1 — Actuadores** | ✅ Garaje = paso a paso (decidido). Falta definir pin del imán/relé de la puerta principal (T0.2). | — |
| **H2 — Bus SPI** | Implementar `spi_master.cpp` (modo 0, maestro). Probar con loopback/lectura de versión del RC522. | H0 |
| **H3 — EEPROM UIDs** | Implementar `eeprom_mgr.cpp` (escribir/leer/buscar/borrar UID + tipo). | — |
| **H4 — RFID** | Implementar `rfid.cpp` sobre SPI: detectar tarjeta, leer UID, leer/escribir contador. | H2 |
| **H5 — Acceso** | `acceso.cpp`: validar UID, abrir imán/garaje, descontar contador de hijo, enrolar/borrar (teclado o serial). | H1,H3,H4 |
| **H6 — Integración** | Descomentar includes en `.ino`, añadir `*_init()`/`*_actualizar()`, comandos serial de enrolamiento, pruebas en Proteus. | H5 |

Las tareas atómicas de H2–H6 están desglosadas en
[`fase-04-rfid-acceso/tasks.md`](fase-04-rfid-acceso/tasks.md).

## Defectos abiertos

| ID | Severidad | Resumen | Archivo |
|----|-----------|---------|---------|
| DEF-001 | Alta | ✅ **Corregido**: horno ahora usa PH5 (HORNO_DDR/HORNO_PORT). Verificar incendio zona 2 al compilar | [`defects/DEF-001-horno-pin-pe5.md`](defects/DEF-001-horno-pin-pe5.md) |
| DEF-002 | Media | 🟡 Parcial: garaje = paso a paso (decidido). Falta puerta principal por imán (Fase 4) | [`defects/DEF-002-actuadores-enunciado.md`](defects/DEF-002-actuadores-enunciado.md) |
| DEF-003 | Baja | ✅ **Corregido**: comentario de `motor.h` actualizado (bloqueante, sin timer) | [`defects/DEF-003-doc-motor-timer3.md`](defects/DEF-003-doc-motor-timer3.md) |
| DEF-004 | Media | ✅ **Corregido**: dimmer con switch Proteus/hardware (brillo pleno en entrenador) | [`defects/DEF-004-dimmer-rango-hardware.md`](defects/DEF-004-dimmer-rango-hardware.md) |
| DEF-005 | Media | ✅ **Corregido**: serial `ARM` armaba sin código → ahora `ARM:1234` (RF-04) | (ver `features/alarma.md` y `001-requisitos.md`) |

## Notas de verificación

- **No fue posible compilar en este entorno** (sin toolchain AVR ni acceso de red para
  instalarlo). Los hallazgos provienen de **análisis estático**: lectura de los módulos
  y un barrido de uso de registros `DDRx/PORTx/PINx`, timers e ISR por archivo.
- Antes de la entrega, **compilar** (Arduino IDE o `arduino-cli`) y ejecutar el plan de
  pruebas del Manual §7, prestando atención especial a la alarma de incendio zona 2 tras
  corregir DEF-001.
