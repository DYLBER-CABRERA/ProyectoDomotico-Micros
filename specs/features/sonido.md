# Feature — Equipo de sonido (volumen analógico)

- **Fase**: 7b · **Estado**: ✅ · **Archivos**: `sonido.cpp`, `sonido.h`

## Objetivo
Encender/apagar el equipo de sonido de forma remota y controlar el volumen mediante una
**señal analógica proporcional** (RF-13).

## Pines / Timer / ADC
- **Entrada de volumen**: potenciómetro en **ADC13 = PK5 = A13** (vía `adc.cpp`).
- **Salida analógica**: PWM en **OC3A = PE3 = D5** (Timer3) → **filtro RC** → voltaje
  analógico proporcional al volumen (lo que exige el enunciado).
- LED "equipo encendido" en **PH6 = D9**.

## API
`sonido_init()`, `sonido_encender()` (enciende; el volumen lo da el pot),
`sonido_actualizar()` (llamar en el loop), `sonido_apagar()`, `sonido_estado()`,
`sonido_volumen()`. Comandos `SONIDO:ON` / `SONIDO:OFF` (teclado `*21#` / `*20#`).

## Comportamiento
El equipo se **enciende/apaga por comando**, pero el **volumen lo fija el potenciómetro
en tiempo real**: `sonido_actualizar()` (llamado cada vuelta del loop) lee A13 (0–1023) y
lo traduce a `OCR3A` (0–255) y a un porcentaje 0–100 para reportes. `sonido_apagar` pone
`OCR3A=0` y apaga el LED. El parámetro de `sonido_encender`/`SONIDO:ON,xx` se ignora (el
pot manda).

## Criterios de aceptación
`SONIDO:ON` (o `*21#`) enciende el LED; girar el potenciómetro de A13 sube/baja el voltaje
tras el filtro RC en tiempo real; `SONIDO:OFF` silencia.
