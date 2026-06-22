# Feature — Alarmas (acceso + incendio)

- **Fase**: 5 · **Estado**: ✅ (DEF-001 corregido; verificar al compilar) · **Archivos**: `alarma.cpp`, `alarma.h`

## Objetivo
Dos subsistemas de alarma independientes con reporte serial y armado/desarmado por código.

## Pines / Interrupciones
- Acceso: SW1→PD2/INT2 (puerta), SW2→PD3/INT3 (ventana). Solo dispara si está **armada**.
- Incendio: SW3→PE4/INT4, SW4→PE5/INT5. **Siempre** activo desde el arranque.
- LEDs: verde armada PB6 (D12), rojo disparada PB7 (D13).
- Disparo por **flanco de subida** (R10k pull-down; al cerrar el switch sube a 5V).

## API
`alarma_init()`, `alarma_armar()`, `alarma_desarmar()`, `alarma_actualizar()` (1 si hay
evento nuevo), `alarma_estado()`, `alarma_tipo_disparo()`, `alarma_verificar_codigo(s)`.

## Comportamiento
Las ISR INT2–INT5 marcan banderas; `alarma_actualizar()` (llamado en `loop()`) las
consume, actualiza LEDs/estado y reporta `ALARMA:ACCESO`/`ALARMA:INCENDIO`. El acceso se
filtra por software (solo si armada). Código por defecto `"1234"` (`alarma.cpp`).

**Armado/desarmado siempre con código** (RF-04): teclado `A`/`B` validan el buffer, y por
serial se usa `ARM:1234` y `DISARM:1234`. *Corregido*: el comando `ARM` antiguo armaba sin
pedir código, lo que violaba el enunciado; ahora exige `ARM:<codigo>` y responde
`ERROR:CODIGO` si no coincide.

**Anti-intrusos (código equivocado)**: cada intento de código fallido (al armar o
desarmar, por teclado o serial) se cuenta en `alarma_registrar_fallo()`. Al llegar a
`ALARMA_MAX_INTENTOS` (=3) se dispara la alarma de **intruso**: estado `DISPARADA`, LED
rojo (PB7) encendido, LCD `!! INTRUSO !!` y alerta serial `ALARMA:INTRUSO`
(tipo `ALARMA_TIPO_INTRUSO`). Un código **correcto** reinicia el contador
(`alarma_reset_intentos()`); para silenciar la alarma de intruso se ingresa el código
correcto (desarmar). El umbral es configurable en `alarma.h`.

## Defecto relacionado
✅ **DEF-001 corregido**: `horno.cpp` ya no usa PE5 (movido a PH5). Verificar al compilar
que la alarma de incendio zona 2 (INT5) dispara con el horno encendido.

## Criterios de aceptación
Manual §7 Pruebas 4 y 5: arm→sensor→disparo→disarm; incendio dispara aun con acceso
desarmado; código incorrecto → `ERROR:CODIGO`.
