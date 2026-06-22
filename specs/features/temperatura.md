# Feature — Control de temperatura

- **Fase**: 6b · **Estado**: ✅ · **Archivos**: `temperatura.cpp`, `temperatura.h`

## Objetivo
Leer un sensor (potenciómetro que simula temperatura) por ADC y controlar calefactor y
ventilador con histéresis.

## Pines / ADC
Entrada: **ADC9 = PK1 = A9** (⚠ no A8), leída por el **módulo `adc.cpp` compartido**.
Calefactor PK3 (A11) y ventilador PK4 (A12) son **salidas digitales on/off**.

## API
`temp_init()` (solo configura PK3/PK4; el ADC lo inicializa `adc_init()`),
`temp_leer_adc()` → `adc_leer(9)`, `temp_celsius()`, `temp_controlar()`.

## Comportamiento
Conversión `temp = ADC*40/1023` (0–40 °C). Histéresis: calefactor ON si <18 °C / OFF si
>20 °C; ventilador ON si >26 °C / OFF si <24 °C. Los LED son **on/off puros, sin variar
el brillo**. La lectura usa el ADC compartido (promedio de 16 muestras + conversión de
descarte al conmutar de canal), lo que evita el parpadeo/cambio de brillo periódico que
se veía antes. `temp_controlar()` corre **siempre** (la seguridad térmica no se pausa).

## Criterios de aceptación
Manual §7 Prueba 7: girar el pot recorre las zonas y enciende/apaga calefactor y
ventilador según los umbrales. Pot en A9.
