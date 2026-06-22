# Constitución del proyecto

Reglas no negociables. Todo cambio de código debe respetarlas; toda excepción debe
documentarse en la spec correspondiente y aprobarse explícitamente.

## Artículo 1 — Plataforma y estilo
1.1. Microcontrolador objetivo: **ATmega2560** (Arduino Mega 2560), reloj **16 MHz**.
1.2. **Programación con registros directos del AVR.** Prohibido usar la API de Arduino
(`digitalWrite`, `pinMode`, `analogRead`, `Serial`, `Servo`, etc.) y librerías de
terceros.
1.3. Cabeceras del sistema permitidas: `<avr/io.h>`, `<avr/interrupt.h>`,
`<util/delay.h>`, `<avr/eeprom.h>`, `<string.h>`, `<ctype.h>`, `<stdint.h>`. Cualquier
otra requiere justificación en la spec.
1.4. `setup()` y `loop()` se mantienen como puntos de entrada (compatibilidad Arduino IDE
y Proteus), pero su cuerpo solo orquesta llamadas a módulos.

## Artículo 2 — Arquitectura modular
2.1. Cada subsistema es un par `modulo.h` / `modulo.cpp`.
2.2. El `.h` expone **solo** la API pública. El estado interno va `static` en el `.cpp`.
2.3. Patrón de API obligatorio: `modulo_init()` (configura hardware), y según el caso
`modulo_actualizar()` (trabajo periódico no bloqueante llamado desde `loop()`),
acciones (`modulo_accion()`) y getters (`modulo_estado()`).
2.4. **Las macros de pin/puerto se definen en el `.h` y se usan en el `.cpp`.** Nunca
escribir el registro literal cuando existe la macro (regla nacida de DEF-001).
2.5. Sin dependencias circulares entre módulos. Si un `.cpp` necesita otra API y crearía
ciclo, usar `extern` declarando solo lo necesario (patrón ya usado por `mercado.cpp`
con las funciones de `usart`).

## Artículo 3 — Concurrencia y tiempo
3.1. El `loop()` es **cooperativo**: ninguna función debe bloquear de forma prolongada,
salvo el movimiento del motor del garaje (bloqueante por diseño y documentado).
3.2. Las ISR deben ser cortas: poner una bandera/dato y retornar. El trabajo se hace en
el `loop()` (`*_actualizar()`).
3.3. `sei()` se llama **una sola vez**, al final de `setup()`, después de todos los
`*_init()`.
3.4. Las temporizaciones por conteo de vueltas del `loop()` (`INTERVALO_TEMP`,
`VUELTAS_POR_SEGUNDO`) son aproximadas y dependen de la velocidad real del `loop`; deben
documentarse como tales y ser calibrables por `#define`.

## Artículo 4 — Recursos de hardware (exclusividad)
4.1. Antes de asignar un pin, timer o vector de interrupción, verificar la tabla en
`003-arquitectura.md`. **Un recurso = un dueño.** Las colisiones son defectos de
severidad alta.
4.2. Asignación de timers vigente: **Timer1 = dimmer**, **Timer2 = teclado**,
**Timer3 = sonido**. El motor del garaje es bloqueante (no usa timer). Fase 4 no debe
apropiarse de estos timers sin reasignación documentada.

## Artículo 5 — EEPROM
5.1. El mapa de memoria EEPROM es único y está definido en `eeprom_mgr.h` /
`003-arquitectura.md`. Las zonas no se solapan: UIDs `0x000–0x09F`, código de alarma
`0x0A0–0x0A3`, mercado `0x0B0–0x1FF`.
5.2. Minimizar escrituras (la EEPROM del AVR tolera ~100k ciclos): leer antes de
escribir y solo escribir si el valor cambia.

## Artículo 6 — Serial
6.1. 9600 bps, 8N1. Entrada normalizada a mayúsculas antes de parsear.
6.2. Toda respuesta es `OK:...` o `ERROR:...` terminada en newline. Las alertas
asíncronas (`ALARMA:*`, `HORNO:FIN`) siguen el mismo formato.
6.3. Comandos nuevos se añaden al parser del `.ino` y se documentan en
`003-arquitectura.md` §protocolo.

## Artículo 7 — Idioma y documentación
7.1. Identificadores y comentarios en **español**, sin tildes en identificadores.
7.2. Mantener el estilo de comentarios denso que explica el *porqué* de cada acceso a
registro.
7.3. Todo cambio de comportamiento actualiza su spec en `features/` y el estado en
`002-estado-y-roadmap.md` **en el mismo commit**.

## Artículo 8 — Definición de "Terminado"
Una fase/módulo está terminado cuando: (a) compila sin warnings de pines/registros,
(b) pasa su plan de pruebas manual en Proteus (`Manual_ProyectoDomotico.md` §7), (c) su
spec refleja el comportamiento real, (d) no introduce colisiones de recursos.
