# DEF-004 — El dimmer no alcanza brillo pleno en hardware físico

- **Severidad**: Media (cumplimiento de RF-10 en el entrenador).
- **Estado**: ✅ Resuelto (switch de compilación).
- **Módulos**: `dimmer.cpp`, `dimmer.h`.

## Síntoma

`dimmer_set()` usaba siempre `OCR1A = nivel * 2`, de modo que el nivel máximo (10) producía
un duty cycle de solo **7.8 %**. Es un truco para que el cambio sea visible en Proteus,
pero en el **entrenador físico** la luz nunca pasa de ~8 % de brillo, incumpliendo el
requisito de iluminación dimerizada plena (RF-10). El Manual §8 afirmaba "en hardware el
rango completo es visible", lo cual no era cierto con esa fórmula.

## Corrección aplicada

Se parametrizó el mapeo con un switch de compilación en `dimmer.h`:

```c
#define DIMMER_MODO_PROTEUS   // por defecto: rango comprimido visible en simulacion
```

En `dimmer.cpp`:
- Con `DIMMER_MODO_PROTEUS` → `OCR1A = nivel*2` (0–7.8 %, comportamiento previo, intacto
  para no romper la simulación).
- Sin esa macro (modo hardware) → `OCR1A = nivel*255/10` (0–100 %, rango pleno).

También se eliminó la variable `valor_ocr` que quedaba sin usar (evita el warning de
compilación, constitución Art. 8).

## Uso

- **Para Proteus**: dejar el `#define DIMMER_MODO_PROTEUS` (estado actual).
- **Para el entrenador**: comentar ese `#define` en `dimmer.h`, o compilar con
  `-DDIMMER_MODO_HARDWARE` y quitar el `#define`, antes de grabar el firmware.

## Criterios de aceptación

1. En Proteus el comportamiento del brillo es idéntico al anterior (niveles 0–10 visibles).
2. En modo hardware, nivel 10 → 100 % de duty (LED a brillo pleno) y nivel 5 → ~50 %.
3. Sin warnings de variable sin usar en `dimmer.cpp`.
