# Mapeo de pines — Sistema Domótico (ATmega2560 / Arduino Mega 2560)

_Actualizado tras los cambios: servomotor en el garaje, potenciómetro de volumen y ADC
compartido._

## Cambios recientes (resumen)

- 🔁 **Garaje**: ya **no** usa motor paso a paso. Ahora es un **servomotor** por PWM.
  → Se liberan los pines **PG0–PG3 (D41–D38)**.
- 🆕 **Volumen del equipo de sonido**: ahora por **potenciómetro** (entrada analógica).
- 🆕 **ADC compartido**: temperatura (A9) y volumen (A0) usan el mismo ADC.

## Entradas

| Señal | Pin Arduino | Puerto AVR | Tipo | Notas |
|-------|-------------|------------|------|-------|
| **Potenciómetro de temperatura** | **A9** | PK1 (ADC9) | Analógica | Extremos a 5V y GND, centro al pin |
| **Potenciómetro de volumen** | **A13** | PK5 (ADC13) | Analógica | Extremos a 5V y GND, centro al pin |
| Teclado 4×4 — filas | D37,D36,D35,D34 | PC0–PC3 | Entrada pull-up | |
| Sensor puerta (SW1) | D19 | PD2 / INT2 | Entrada | alarma acceso |
| Sensor ventana (SW2) | D18 | PD3 / INT3 | Entrada | alarma acceso |
| Sensor humo 1 (SW3) | D2 | PE4 / INT4 | Entrada | alarma incendio |
| Sensor humo 2 (SW4) | D3 | PE5 / INT5 | Entrada | alarma incendio |
| USART RX | D0 | PE0 | Entrada | terminal serial |

## Salidas

| Señal | Pin Arduino | Puerto AVR | Tipo | Notas |
|-------|-------------|------------|------|-------|
| **Servo garaje** | **D6** | PH3 (OC4A, Timer4) | PWM 50 Hz | Proteus: servo; entrenador: LED + R220 |
| **Volumen (salida analógica)** | **D5** | PE3 (OC3A, Timer3) | PWM → filtro RC | señal analógica proporcional al volumen |
| LED equipo de sonido (ON) | D9 | PH6 | Digital | indicador encendido |
| Dimmer iluminación | D11 | PB5 (OC1A, Timer1) | PWM (curva log) | LED + R220 |
| Calefactor | A11 | PK3 | Digital on/off | LED rojo |
| Ventilador | A12 | PK4 | Digital on/off | LED azul |
| LED horno | D8 | PH5 | Digital | simula relé del horno |
| LED alarma armada (verde) | D12 | PB6 | Digital | |
| LED alarma disparada (rojo) | D13 | PB7 | Digital | también "sirena" de intruso |
| USART TX | D1 | PE1 | Salida | terminal serial |
| LCD 16×2 | D22,D24,D26–D29 | PA0,PA2,PA4–PA7 | Digital | RS,EN,D4–D7 |
| Teclado 4×4 — columnas | D49–D46 | PL0–PL3 | Salida | |

## Timers (asignación)

| Timer | Uso |
|-------|-----|
| Timer1 | Dimmer (OC1A / D11) |
| Timer2 | Escaneo del teclado (CTC) |
| Timer3 | PWM volumen sonido (OC3A / D5) |
| **Timer4** | **PWM servo garaje (OC4A / D6)** — nuevo |
| Timer5 | Libre (disponible para Fase 4 si hiciera falta) |

## ADC (canales)

| Canal | Pin | Uso |
|-------|-----|-----|
| ADC9 | A9 (PK1) | Potenciómetro de temperatura |
| ADC13 | A13 (PK5) | Potenciómetro de volumen |

> El módulo `adc.cpp` conmuta el multiplexor en cada lectura y descarta una conversión
> tras cambiar de canal (evita que las lecturas se contaminen entre A9 y A13, que era una
> de las causas del parpadeo de los LEDs de temperatura).

## Pines liberados

- **PG0–PG3 (D41–D38)**: antes el motor paso a paso del garaje; ahora **libres**.

## Pendiente de definir (Fase 4)

- Imán/relé de la puerta principal: un pin digital libre (p. ej. del Puerto G ya liberado,
  o Puerto J).
- SPI del RC522: PB0–PB3 (D53/D52/D51/D50) + RST/CS.
