# Feature — Teclado matricial 4×4

- **Fase**: 2 · **Estado**: ✅ · **Archivos**: `teclado.cpp`, `teclado.h`

## Objetivo
Leer un teclado 4×4 por escaneo periódico con Timer2, sin bloquear el `loop()` ni usar
librerías.

## Pines / Timer
Filas (entrada + pull-up interno): PC0–PC3 (D37–D34). Columnas (salida): PL0–PL3
(D49–D46). Escaneo por **Timer2** en CTC con ISR `TIMER2_COMPA_vect`.

## API
`teclado_init()`, `teclado_hay()` (1 si hay tecla nueva), `teclado_leer()` (devuelve el
char y consume la bandera).

## Comportamiento
La ISR de Timer2 recorre columnas activando LOW y lee filas; con pull-up interno
(`PORTC |= 0x0F`) la fila en reposo está HIGH y cae a LOW al pulsar. La tecla se
deposita en una variable que `teclado_leer()` consume. Mapa de funciones (en el `.ino`):
0–9 acumulan código; A=armar, B=desarmar, C=subir luz, D=bajar luz.

**Modo comando** (entrenador sin terminal): `*` activa la captura de un comando
`‹código2díg›‹params›` que se ejecuta con `#` (`A` separa parámetros, `*` cancela). Permite
operar sonido, horno, garaje, luz por valor y mercado solo con el teclado. La máquina de
estados está en `procesar_tecla_alarma()` y el despacho en `ejecutar_comando_teclado()`.
Tabla completa: [`../../COMANDOS_TECLADO.md`](../../COMANDOS_TECLADO.md).

## Criterios de aceptación
Manual §7 Prueba 2: pulsar dígitos muestra `*` en LCD; C/D ajustan la luz; A/B operan la
alarma. Responde mientras corre el resto del sistema.
