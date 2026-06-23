# 003 — Arquitectura del sistema

## Visión general

Firmware de bucle único cooperativo. `setup()` inicializa cada módulo y habilita
interrupciones; `loop()` atiende, en orden de prioridad, alarmas → teclado → serial →
temperatura/horno. Los eventos asíncronos (sensores, teclado, recepción serial) se
capturan en ISRs cortas que dejan banderas/datos que el `loop()` consume.

```
        ┌─────────────────────── ISRs (asíncronas) ───────────────────────┐
        │ INT2/3 (acceso) INT4/5 (incendio) TIMER2 (teclado) USART0_RX     │
        └───────────────┬──────────────────────────────────────┬──────────┘
                        │ banderas/datos                        │
   setup(): *_init() ; sei()                                    │
                        ▼                                        ▼
   loop(): alarma_actualizar() → teclado_hay/leer → usart_hay_linea/procesar
           → (cada INTERVALO_TEMP) temp_celsius/controlar → horno_actualizar()
                        │
                        ▼ salidas
        LCD · LEDs alarma · calefactor/ventilador · dimmer PWM · motor · horno · sonido
```

## Mapa de pines (autoritativo)

| Pin Mega | Puerto AVR | Dir | Función | Módulo |
|----------|-----------|-----|---------|--------|
| D22 | PA0 | OUT | LCD RS | lcd |
| D24 | PA2 | OUT | LCD EN | lcd |
| D26–D29 | PA4–PA7 | OUT | LCD D4–D7 | lcd |
| D37–D34 | PC0–PC3 | IN pull-up | Teclado filas 0–3 | teclado |
| D49–D46 | PL0–PL3 | OUT | Teclado columnas 0–3 | teclado |
| D0 | PE0 | IN | USART0 RX | usart |
| D1 | PE1 | OUT | USART0 TX | usart |
| D19 | PD2 | IN | INT2 — SW1 puerta (acceso) | alarma |
| D18 | PD3 | IN | INT3 — SW2 ventana (acceso) | alarma |
| D2 | PE4 | IN | INT4 — SW3 humo zona 1 | alarma |
| D3 | PE5 | IN | INT5 — SW4 humo zona 2 | alarma |
| D12 | PB6 | OUT | LED verde (armada) | alarma |
| D13 | PB7 | OUT | LED rojo (disparada) | alarma |
| D6 | PH3 | OUT PWM | Garaje: **servomotor** (OC4A, Timer4, 50 Hz) | motor |
| A9 | PK1 | IN ADC9 | Potenciómetro temperatura | temperatura/adc |
| A13 | PK5 | IN ADC13 | Potenciómetro **volumen** | sonido/adc |
| A11 | PK3 | OUT | Calefactor (LED, on/off) | temperatura |
| A12 | PK4 | OUT | Ventilador (LED, on/off) | temperatura |
| D11 | PB5 | OUT PWM | Dimmer (OC1A, Timer1, curva log) | dimmer |
| D5 | PE3 | OUT PWM | Sonido volumen (OC3A, Timer3) → RC | sonido |
| D9 | PH6 | OUT | LED equipo de sonido | sonido |
| D8 | PH5 | OUT | LED horno (rele) | horno |
| D41 | PG0 | OUT | Imán/relé puerta principal | acceso |
| D52 | PB1 | OUT | SCK SPI (RC522) | spi_master |
| D51 | PB2 | OUT | MOSI SPI | spi_master |
| D50 | PB3 | IN | MISO SPI | spi_master |
| D53 | PB0 | OUT | CS / SS SPI (RC522) | spi_master |

> ✅ **DEF-001 corregido**: `horno.cpp` usa PH5 mediante las macros `HORNO_DDR`/
> `HORNO_PORT` de `horno.h` (antes escribía PE5, que es el sensor de incendio SW4/INT5).
> ✅ **DEF-002 (garaje)**: servomotor por PWM en **D6/PH3 (Timer4)**, no bloqueante.
> El garaje es servo; la puerta principal usa imán en **PG0 (D41)**. Ambos implementados
> en `acceso.cpp`.
> 🆕 **ADC compartido** (`adc.cpp`): temperatura en ADC9 (A9) y volumen en ADC13 (A13/PK5),
> con conmutación de mux y conversión de descarte por lectura.

### SPI y Fase 4 (RFID RC522)

SPI por hardware del Mega en **Puerto B**: `SS=PB0 (D53)`, `SCK=PB1 (D52)`, `MOSI=PB2 (D51)`,
`MISO=PB3 (D50)`. PB0–PB3 están libres (PB5–PB7 los usan dimmer y LEDs de alarma).
PB0 se usa como CS del RC522. No se necesita RST por separado (se hace soft-reset por SPI).

## Timers e interrupciones

| Recurso | Dueño | Configuración |
|---------|-------|---------------|
| Timer1 | dimmer | Fast PWM en OC1A (PB5) |
| Timer2 | teclado | CTC, ISR `TIMER2_COMPA_vect` para escaneo |
| Timer3 | sonido | Fast PWM 8 bits en OC3A (PE3), prescaler 8 |
| **Timer4** | **servo garaje** | Fast PWM TOP=ICR4, 50 Hz, OC4A (PH3/D6) |
| INT2/INT3 | alarma acceso | flanco de subida (EICRA) |
| INT4/INT5 | alarma incendio | flanco de subida (EICRB) |
| USART0_RX | usart | recepción a buffer circular |

> Timer0 y Timer5 quedan **libres**. El servo del garaje es **no bloqueante** (solo fija
> el ancho de pulso); ya no hay ninguna operación bloqueante con `_delay_ms` en el flujo
> normal (el mercado se lista por serial sin `_delay`).

## Mapa de memoria EEPROM

| Rango | Contenido | Definido en |
|-------|-----------|-------------|
| `0x000–0x09F` | Hasta 10 UIDs autorizados (4 bytes UID + 1 byte tipo) | `eeprom_mgr.h` (Fase 4) |
| `0x0A0–0x0A3` | Código de alarma (4 dígitos) | `eeprom_mgr.h` (Fase 4) |
| `0x0B0–0x1FF` | Lista de mercado (10 items × 14 bytes) | `mercado.h` (activo) |

> El módulo `mercado` ya usa `0x0B0+`. Fase 4 debe respetar las zonas de UIDs/código sin
> solaparse con mercado.

## Protocolo serial (9600 8N1)

Entrada normalizada a mayúsculas. Comando terminado en `\n`.

| Comando | Acción | OK | ERROR |
|---------|--------|----|----|
| `ARM:1234` | Armar acceso (**requiere código**, RF-04) | `OK:ARMADA` | `ERROR:CODIGO` |
| `DISARM:1234` | Desarmar con código | `OK:DESACTIVADA` | `ERROR:CODIGO` |
| `LUZ:0..10` | Dimmer | `OK:LUZ,N` | — |
| `GARAJE:ABRIR` / `CERRAR` | Motor garaje | `OK:GARAJE_ABRIENDO`→`OK:GARAJE_ABIERTO` | — |
| `HORNO:temp,min` | Encender horno | `OK:HORNO,temp,min` | — |
| `SONIDO:ON,vol` / `SONIDO:OFF` | Equipo de sonido | `OK:SONIDO_ON,vol` / `OK:SONIDO_OFF` | — |
| `MERCADO:ADD,nom,cant` | Agregar producto | `OK:MERCADO_AGREGADO` | `ERROR:MERCADO_LLENO_O_DUPLICADO` / `ERROR:FORMATO_MERCADO` |
| `MERCADO:DEL,nom` | Eliminar | `OK:MERCADO_ELIMINADO` | `ERROR:MERCADO_NO_ENCONTRADO` |
| `MERCADO:LIST` | Listar | (lista) | `LISTA VACIA` |
| `ENROL:ADULTO,<cod>` | Enrolar próxima tarjeta como adulto | `OK:ENROLANDO_ADULTO` → `OK:ENROLADO_ADULTO` | `ERROR:CODIGO` / `ERROR:EEPROM_LLENA_O_DUPLICADO` |
| `ENROL:HIJO,<n>,<cod>` | Enrolar hijo con N cupos | `OK:ENROLANDO_HIJO,N` → `OK:ENROLADO_HIJO,N` | idem |
| `BORRAR,<cod>` | Borrar próxima tarjeta presentada | `OK:BORRANDO` → `OK:BORRADO` | `ERROR:CODIGO` / `ERROR:UID_NO_EXISTE` |
| `ACCESOS:<n>,<cod>` | Recargar N accesos (presentar tarjeta hijo) | `OK:RECARGANDO,N` → `OK:ACCESOS,N` | `ERROR:CODIGO` / `ERROR:UID_NO_EXISTE` / `ERROR:UID_NO_ES_HIJO` |
| (otro) | — | — | `ERROR:COMANDO_DESCONOCIDO` |

Alertas asíncronas: `ALARMA:INCENDIO`, `ALARMA:ACCESO`, `ALARMA:INTRUSO` (3 códigos
equivocados seguidos), `HORNO:FIN`, `ACCESO:CONCEDIDO_ADULTO`, `ACCESO:CONCEDIDO_HIJO,N`,
`ACCESO:DENEGADO`, `ACCESO:DENEGADO_SIN_CUPOS`.

### Comandos por teclado (entrenador sin terminal)

El entrenador físico solo tiene teclado, así que cada función serial tiene un equivalente
por teclado mediante un **modo comando**: `*` inicia, `‹FF›` (código de función de 2
dígitos) + parámetros (`A` separa parámetros), `#` ejecuta, `*` cancela. Implementado en
`procesar_tecla_alarma()` + `ejecutar_comando_teclado()` del `.ino`. Tablas completas y
catálogo de productos del mercado en **[`../COMANDOS_TECLADO.md`](../COMANDOS_TECLADO.md)**.
Resumen de códigos: `1x` garaje, `2x` sonido, `3x` horno, `41` luz, `5x` mercado,
`60` borrar, `61` enrol adulto, `62` enrol hijo, `9x` alarma.
Teclas directas: `A`/`B` armar/desarmar (con código), `C`/`D` luz ±.

## Layout del LCD

- **Línea 0**: estado de alarma (`ALARMA: DESACTIV` / `ARMADA` / `!! INCENDIO !!` /
  `!! INTRUSION !!`) — también la usan horno (`HORNO: ON/LISTO`) puntualmente.
- **Línea 1**: izquierda `T:NNc` (temperatura), derecha `L:N/10` (luz). Mientras se
  digita el código, `Codigo: ****` ocupa toda la línea 1 (lógica de `ultima_interaccion`
  en el `.ino` evita que la temperatura la pise).
