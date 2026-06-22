# DEF-002 — Actuadores de acceso no coinciden con el enunciado

- **Severidad**: Media (cumplimiento de requisito RF-07).
- **Estado**: 🟡 Garaje **resuelto con servomotor** (coincide con el enunciado). Queda
  pendiente solo la puerta principal por imán (Fase 4).
- **Módulos**: `motor.*` (ahora servo), `acceso.*` (stub), Proteus.

## Actualización (servo)

Se **reemplazó el motor paso a paso por un servomotor** (lo que pide el enunciado):
PWM 50 Hz en **D6 / PH3 (OC4A, Timer4)**, no bloqueante. En Proteus se usa un servo; en el
entrenador, un LED en D6. Los pines PG0–PG3 quedaron libres. Ver `motor.cpp`/`motor.h` y
`MAPEO_PINES.md`. Falta únicamente el actuador de la **puerta principal (imán/relé)**, que
se hará en la Fase 4 (`acceso.cpp`).

## Decisión tomada (2026-06-22)

El equipo decidió **mantener el motor paso a paso** para el garaje (Opción B): se conserva
`motor.cpp` tal cual (ya funciona) y NO se migra a servomotor. Justificación: el módulo
está implementado y probado, y el entrenador/montaje usa el paso a paso. Esta desviación
respecto al texto del enunciado ("servomotor") debe **confirmarse/justificarse ante el
docente** en la entrega.

**Sigue pendiente (no opcional):** implementar la **puerta principal por imán/relé** —
una salida digital simple controlada desde `acceso.cpp`. Se aborda en la Fase 4 (ver
`fase-04-rfid-acceso/`). Por tanto este defecto se cierra solo cuando exista la puerta
principal; la parte del garaje ya está decidida.

## Síntoma

El enunciado (RF-07) especifica:

> "El acceso puede ser a través de la puerta principal o el garaje. **La puerta principal
> se controla mediante un imán** y **la puerta garaje se controla mediante un
> servomotor**."

El código actual:

- Controla el **garaje con un motor paso a paso** (`motor.cpp`, secuencia de 4 bobinas en
  Puerto G), no con un servomotor (PWM de ancho de pulso 1–2 ms).
- **No implementa la puerta principal por imán/relé** (la API existe como stub en
  `acceso.h`: `acceso_abrir_principal()` "activa relé", sin implementación).

## Análisis

Hay dos caminos válidos; debe elegirse uno y documentarse:

**Opción A — Ajustarse literalmente al enunciado**
- Garaje con **servomotor**: PWM 50 Hz, pulso 1–2 ms, sobre un OC libre (Timer4/Timer5
  están libres; p. ej. OC4A en PH3/D6). Sustituye o complementa a `motor.cpp`.
- Puerta principal con **imán/relé**: una salida digital simple (un pin libre del Puerto
  H/J/D) que `acceso_abrir_principal()`/`cerrar_principal()` activan.

**Opción B — Mantener el paso a paso y justificarlo**
- Conservar `motor.cpp` para el garaje (ya funciona) y documentar la desviación como
  decisión de diseño aprobada por el docente.
- Igualmente hay que **implementar la puerta principal por imán** (no es opcional en el
  enunciado).

> El diagrama de Proteus aportado muestra un **servomotor** en la esquina inferior
> izquierda, lo que sugiere que la Opción A es la esperada. Confirmar.

## Decisiones restantes

| Pregunta | Responsable | Estado |
|----------|-------------|--------|
| ¿Garaje con servo o paso a paso? | Equipo | ✅ **Paso a paso** (Opción B) |
| Pin del imán/relé puerta principal | Equipo | ⏳ Pendiente (pin digital libre, Puerto H/J/D) |

## Criterios de aceptación

1. La puerta principal se abre/cierra por su actuador (imán/relé) desde `acceso.cpp`.
2. ✅ El garaje usa el **motor paso a paso** (`motor.cpp`) y se mueve correctamente en
   Proteus (ya cumplido).
3. La decisión (paso a paso) queda registrada en `fase-04-rfid-acceso/spec.md` y en el
   mapa de pines de `003-arquitectura.md`.
4. La desviación respecto al enunciado (servo→paso a paso) se justifica ante el docente.
