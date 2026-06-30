# Sustentación — Historial de Accesos RFID

**Fecha:** 2026-06-24  
**Autores:** Dylber Cabrera · Juan David Ocampo  
**Proyecto:** Sistema Domótico ATmega2560 (Microprocesadores 2026-1)

---

## Qué se agregó

Módulo de **historial de accesos RFID** persistente en EEPROM interna. Cada vez que
una tarjeta enrolada es aceptada, se guarda un registro con el tipo de usuario (adulto
o hijo), el lugar de acceso y el UID de la tarjeta. El historial sobrevive reinicios y
se consulta por el puerto serial.

---

## Archivos nuevos

| Archivo | Función |
|---|---|
| `include/historial.h` | API pública: `historial_init`, `historial_registrar`, `historial_listar`, `historial_limpiar` |
| `src/historial.cpp` | Implementación: ring buffer de 20 entradas en EEPROM `0x200–0x279` |

---

## Archivos modificados

### `include/eeprom_mgr.h`
- Se agregó comentario documentando la zona `0x200–0x279` reservada para el historial.

### `src/rfid.cpp`
- Se incluyó `historial.h`.
- Se agregó `historial_registrar(...)` en los **4 puntos de acceso concedido**:
  1. **SALA + adulto** → `HIST_TIPO_ADULTO, LUGAR_SALA`
  2. **SALA + hijo** (cuando el descuento fue exitoso) → `HIST_TIPO_HIJO, LUGAR_SALA`
  3. **GARAJE** → tipo leído de EEPROM + `LUGAR_GARAJE`
  4. **PUERTA** → tipo leído de EEPROM + `LUGAR_PUERTA`
- Los accesos **denegados no se registran** (tarjeta desconocida, sin cupos).

### `ProyectoDomotico.ino`
- `#include "include/historial.h"` agregado junto a los demás módulos.
- `historial_init()` en `setup()`, después de `eeprom_init()` y antes de `rfid_init()`.
- Nuevos **comandos seriales** en `procesar_comando_serial()`:
  - `HISTORIAL` → lista todos los accesos registrados.
  - `HISTORIAL:LIMPIAR` → borra el historial completo.
- Nuevos **comandos de teclado** en `ejecutar_comando_teclado()`:
  - `*71#` → listar historial por serial + LCD: `HISTORIAL->UART`
  - `*70#` → limpiar historial + LCD: `HIST LIMPIADO`
- Mensaje de bienvenida actualizado con los nuevos comandos.

---

## Diseño del almacenamiento (EEPROM ring buffer)

```
Dirección  Contenido
─────────────────────────────────────────────────────────
0x200      head  (1 byte) — índice del slot más antiguo
0x201      count (1 byte) — número de entradas (0–20)
0x202      Entrada 0: tipo(1) + lugar(1) + uid[4]   = 6 bytes
0x208      Entrada 1: tipo(1) + lugar(1) + uid[4]
...
0x279      Entrada 19 (última)
─────────────────────────────────────────────────────────
Total: 2 + 20×6 = 122 bytes  |  Zona libre a partir de 0x200
```

**Ring buffer FIFO circular**: cuando las 20 entradas están llenas, la siguiente
sobreescribe la más antigua (`head` avanza). Así el historial siempre conserva los
20 accesos más recientes sin consumir más EEPROM.

**Protección de ciclos**: cada byte solo se escribe si el valor cambió (`eeprom_read`
antes de `eeprom_write`), extendiendo la vida útil de la EEPROM.

---

## Formato de salida por serial

Comando: `HISTORIAL`

```
--- HISTORIAL DE ACCESOS ---
ADULTO,PUERTA,AB:CD:EF:12
HIJO,SALA,34:56:78:9A
ADULTO,GARAJE,AB:CD:EF:12
```

Columnas: `TIPO,LUGAR,UID_HEX`

- `TIPO`  → `ADULTO` o `HIJO`
- `LUGAR` → `PUERTA`, `GARAJE` o `SALA`
- `UID`   → 4 bytes en hexadecimal separados por `:`

---

## Comandos disponibles

| Comando serial | Teclado | Acción |
|---|---|---|
| `HISTORIAL` | `*71#` | Lista el historial completo por UART |
| `HISTORIAL:LIMPIAR` | `*70#` | Borra todas las entradas |

---

## Lo que NO se cambió

- El flujo de acceso RFID existente (apertura de imán, servo, contador de cupos) no
  fue modificado: solo se agregaron llamadas después de cada resultado de éxito.
- Los prescalers de Timer2, la lógica MIFARE, el mapa de pines y los patrones de ISR
  quedan intactos.
- El historial no usa RAM dinámica ni bloquea el loop: las escrituras EEPROM (~3 ms
  por byte × 8 bytes = ~24 ms) ocurren en el mismo contexto de `manejar_tarjeta_ok()`,
  que ya era lento por el protocolo SPI del RC522.
