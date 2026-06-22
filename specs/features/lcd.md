# Feature — LCD 16×2 (HD44780, modo 4 bits)

- **Fase**: 1 · **Estado**: ✅ · **Archivos**: `lcd.cpp`, `lcd.h`

## Objetivo
Mostrar estado de alarma, temperatura, nivel de luz y mensajes en una pantalla 16×2
controlada por registros del Puerto A, sin librerías.

## Pines (Puerto A)
RS=PA0 (D22), EN=PA2 (D24), D4–D7=PA4–PA7 (D26–D29). RW a GND fijo. Modo 4 bits
(D0–D3 del LCD sin conectar).

## API
`lcd_init()`, `lcd_clear()`, `lcd_goto(fila,col)`, `lcd_string(s)`, `lcd_char(c)`,
`lcd_int(n)`.

## Comportamiento
`lcd_init()` ejecuta la secuencia de arranque del HD44780 (tiempos con `_delay_ms/us`).
Layout del LCD (ver `003-arquitectura.md`): línea 0 = estado de alarma; línea 1 =
`T:NNc` (izq) + `L:N/10` (der), o `Codigo: ****` ocupando toda la línea al digitar.

## Criterios de aceptación
Manual §7 Prueba 1: al iniciar muestra el estado inicial; tras ~2 s aparece la
temperatura. Sin parpadeos ni caracteres basura.
