# Feature — USART0 (serial)

- **Fase**: 3 · **Estado**: ✅ · **Archivos**: `usart.cpp`, `usart.h`

## Objetivo
Comunicación serial bidireccional a 9600 8N1 para comandos remotos y alertas, con
recepción por interrupción a buffer circular.

## Pines / Interrupción
RX0=PE0 (D0), TX0=PE1 (D1). ISR `USART0_RX_vect` para recepción.

## API
`usart_init()`, `usart_enviar_string(s)`, `usart_enviar_int(n)`, `usart_enviar_newline()`,
`usart_hay_linea()`, `usart_leer_linea(buf)`.

## Comportamiento
`usart_init()` configura UBRR para 9600 bps, 8N1, habilita TX/RX y `RXCIE0`. La ISR
acumula bytes en un buffer circular (tamaño `USART_RX_BUF_SIZE`, 32). `usart_hay_linea()`
es verdadero cuando hay `\n`/`\r`; `usart_leer_linea()` copia hasta el fin de línea y
agrega `\0`. El `.ino` normaliza a mayúsculas antes de parsear.

## Criterios de aceptación
Manual §7 Prueba 3: la terminal recibe el menú al iniciar; los comandos responden
`OK:`/`ERROR:`. Requiere TX/RX **cruzados** y 9600 bps.
