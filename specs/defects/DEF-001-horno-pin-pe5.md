# DEF-001 — El horno maneja PE5 en lugar de PH5 (colisión con alarma de incendio)

- **Severidad**: Alta (compromete un requisito de seguridad: RF-02).
- **Estado**: Abierto.
- **Módulos**: `horno.cpp`, `horno.h`, `alarma.cpp`, `ProyectoDomotico.ino`.

## Síntoma

`horno_init()` y las funciones de encendido/apagado del horno configuran como **salida y
escriben el Puerto E, bit 5 (PE5)**. PE5 es la entrada **INT5 / sensor de humo SW4
(incendio zona 2)**. Al encender el horno, se fuerza ese pin como salida, anulando la
detección de incendio de la zona 2 (y potencialmente disparando la ISR de forma
espuria).

## Causa raíz

`horno.h` define correctamente las macros del Puerto H:

```c
#define HORNO_DDR    DDRH
#define HORNO_PORT   PORTH
#define HORNO_PIN    PH5
```

pero `horno.cpp` **no las usa** y escribe el registro literal del Puerto E:

```c
DDRE  |= (1 << HORNO_PIN);   // ← debería ser HORNO_DDR  (DDRH)
PORTE |= (1 << HORNO_PIN);   // ← debería ser HORNO_PORT (PORTH)
PORTE &= ~(1 << HORNO_PIN);  // ← idem
```

Como `HORNO_PIN` vale numéricamente 5, `(1 << HORNO_PIN)` es el bit 5; aplicado a `DDRE`/
`PORTE` afecta a **PE5**. El comentario de cabecera del propio `horno.h` ya advertía que
PE5 estaba ocupado y que el LED debía moverse a PH5: el `.cpp` quedó a medio migrar.

El `.ino` lo confirma en `setup()`: `horno_init(); // Fase 7a: LED horno en PE5 (pin 3)`.

## Corrección propuesta

En `horno.cpp`, reemplazar los accesos literales por las macros del header:

```c
// horno_init()
HORNO_DDR  |= (1 << HORNO_PIN);   // PH5 como salida
HORNO_PORT &= ~(1 << HORNO_PIN);  // apagado al inicio

// horno_encender()
HORNO_PORT |= (1 << HORNO_PIN);

// horno_apagar()
HORNO_PORT &= ~(1 << HORNO_PIN);
```

Actualizar el comentario en `ProyectoDomotico.ino` (`setup()`) de "PE5 (pin 3)" a
"PH5 (pin 8)". Conectar el LED del horno en Proteus a **D8/PH5**.

## Criterios de aceptación

1. Encender el horno (`HORNO:180,25`) **no** afecta el LED ni la lógica de la alarma de
   incendio.
2. La alarma de incendio zona 2 (SW4 → INT5) sigue disparando con el horno encendido
   (Manual §7 Prueba 5, repetida con horno ON).
3. El LED del horno enciende/apaga en D8/PH5.
4. `grep -n "PORTE\|DDRE" horno.cpp` no devuelve resultados.
