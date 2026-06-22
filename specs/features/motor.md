# Feature — Garaje (servomotor)

- **Fase**: 6a · **Estado**: ✅ (servo, no bloqueante) · **Archivos**: `motor.cpp`, `motor.h`

## Objetivo
Abrir/cerrar el garaje con un **servomotor** por comando (serial o teclado), sin bloquear
el resto del sistema.

## Pin / Timer
Señal del servo en **OC4A = PH3 = D6**, PWM **50 Hz** generado por **Timer4** (Fast PWM,
TOP=ICR4=39999, prescaler 8). Anchos de pulso: cerrado ~1.0 ms (OCR4A=2000), abierto
~2.0 ms (OCR4A=4000).
- **Proteus**: conectar un servo a D6.
- **Entrenador**: conectar un LED + R220 a D6 (brilla según la posición).

## API
`motor_init()`, `garaje_abrir()`, `garaje_cerrar()`, `garaje_estado()`
(`GARAJE_CERRADO`/`GARAJE_ABIERTO`).

## Comportamiento
`garaje_abrir`/`garaje_cerrar` solo cambian `OCR4A`; el servo se mueve solo. **No
bloqueante** (a diferencia del antiguo paso a paso): el teclado, las alarmas y el mercado
siguen respondiendo durante el movimiento.

## Historial
Antes era un **motor paso a paso** (Puerto G, bloqueante con `_delay_ms`). Se reemplazó
por servo (DEF-002) a pedido del proyecto; PG0–PG3 quedaron libres.

## Criterios de aceptación
`GARAJE:ABRIR`/`CERRAR` (o `*11#`/`*10#`) mueven el servo en Proteus y el sistema sigue
atendiendo otras funciones simultáneamente.
