# DEF-003 — Documentación desactualizada: `motor.h` menciona Timer3

- **Severidad**: Baja (solo documentación; no afecta ejecución).
- **Estado**: Abierto.
- **Módulo**: `motor.h`.

## Síntoma

El comentario de cabecera de `motor.h` afirma:

> "Se usa Timer3 para el delay entre pasos sin bloquear el resto del sistema con
> `_delay_ms` largos."

Pero `motor.cpp` **no** configura Timer3: el avance de pasos es **bloqueante** mediante
`_delay_ms(5)` (así lo confirman el `.ino` y el Manual §7 Prueba 6). Timer3 lo usa en
realidad `sonido.cpp` (PWM de volumen).

## Riesgo

Confusión para quien implemente Fase 4: podría creer que Timer3 está reservado por el
motor o que el motor es no bloqueante. Una asignación errónea de timers basada en este
comentario causaría una colisión real con `sonido`.

## Corrección propuesta

Actualizar el comentario de `motor.h` para reflejar que el movimiento es **bloqueante por
diseño** (`_delay_ms` entre pasos) y que **no usa ningún timer**. Si en el futuro se
migra a un avance no bloqueante, usar un timer **libre** (Timer4 o Timer5), nunca Timer3.

## Criterios de aceptación

1. El comentario de `motor.h` describe el comportamiento real (bloqueante, sin timer).
2. `003-arquitectura.md` marca Timer3 como exclusivo de `sonido` y Timer4/5 como libres
   (ya reflejado).
