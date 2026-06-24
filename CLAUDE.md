# CLAUDE.md

La guía de contexto de este proyecto es **[AGENTS.md](AGENTS.md)** — léela COMPLETA antes
de hacer cualquier cosa. Contiene la arquitectura, el mapa de pines, los comandos, las
reglas de oro y, sobre todo, la sección **"Lecciones aprendidas (landmines)"** con los
errores que ya se cometieron y NO se deben repetir.

@AGENTS.md

## Recordatorio rápido (lo más crítico)

- Proyecto **ATmega2560 en registros directos, SIN librerías de Arduino**. YA FUNCIONA y
  fue sustentado. La profesora califica **funciona / no funciona (0/5)** → **la regla #1
  es NO ROMPER lo existente**: cambios pequeños, acotados, respetando los patrones.
- **No compilas tú**: el usuario compila y sube por COM13. Verifica releyendo tus
  ediciones; el usuario prueba en hardware.
- **Tiempo = `millis_sistema()`** (reloj por Timer2), nunca contando vueltas del loop.
- **Timer2 prescaler = /1024** (`CS22|CS21|CS20`), no /128.
- **LCD**: línea 0 = alarma/eventos, línea 1 = temp(0–8)/luz(9–15). Si se corrompe,
  `lcd_resync()` lo recupera. No repintes la línea 1 sin re-sincronizar.
- **RFID**: lectura de registro con bit 7 (`|0x80`); MIFARE READ/WRITE con CRC.
- **Comando nuevo** = un `case`/`else if` en `ejecutar_comando_teclado()` /
  `procesar_comando_serial()` del `.ino`.
- Detalle de comandos en `doc/COMANDOS_TECLADO.md`; especificaciones en `specs/`.
