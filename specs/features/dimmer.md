# Feature — Dimmer de iluminación

- **Fase**: 6c · **Estado**: ✅ · **Archivos**: `dimmer.cpp`, `dimmer.h`

## Objetivo
Regular el brillo de la iluminación por PWM, ajustable por teclado (C/D) o serial
(`LUZ:N`).

## Pin / Timer
Salida PWM en **OC1A = PB5 = D11**, generada por **Timer1** (Fast PWM).

## API
`dimmer_init()`, `dimmer_set(nivel)`, `dimmer_get()`, `DIMMER_NIVEL_MAX = 10`.

## Comportamiento
Escala de usuario **0–10**. `dimmer_set` aplica una **curva de brillo logarítmica /
perceptual** mediante una tabla (`curva[11]`) que mapea cada nivel a `OCR1A` (0–255). El
duty crece de forma ~exponencial (×1.7 por paso: 0,2,4,8,14,25,44,78,124,180,255) porque
el ojo percibe el brillo de forma logarítmica; así cada nivel se ve como un paso parejo y
el voltaje promedio entra de forma gradual. Cubre todo el rango (apto para Proteus y
entrenador). El `.ino` muestra `L:N/10` y reporta `LUZ:N` / `OK:LUZ,N`.

## Criterios de aceptación
Manual §7 Prueba 8: C/D y `LUZ:` cambian el nivel; `LUZ:0` apaga; el brillo sube de forma
suave y perceptualmente uniforme entre niveles. La tabla `curva[]` en `dimmer.cpp` es
ajustable.
