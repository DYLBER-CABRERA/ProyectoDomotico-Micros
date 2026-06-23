# AGENTS.md — Guía operativa para agentes

> Sistema domótico sobre **ATmega2560 (Arduino Mega 2560)**, escrito con **registros
> directos del AVR, sin librerías de Arduino**. Proyecto final de Microprocesadores,
> 2026-1. Autores: **Dylber Cabrera** y **Juan David Ocampo**.
>
> Este archivo es el punto de entrada para cualquier agente (Claude, Copilot, etc.).
> Las especificaciones detalladas viven en [`specs/`](specs/). **Lee
> [`specs/constitution.md`](specs/constitution.md) antes de escribir código.**

---

## 1. Qué es este proyecto

Firmware monolítico (un solo `loop()` cooperativo + ISRs) que controla la seguridad y
el confort de una vivienda:

- **Seguridad**: alarma de acceso (puertas/ventanas) y alarma de incendio (humo), con
  reporte por puerto serial y armado/desarmado por código.
- **Acceso**: tarjetas RFID, puerta principal por imán y garaje por motor, con
  contadores de acceso por hijo a la sala de juegos. *(Fase 4 — pendiente)*
- **Confort**: iluminación dimerizada, control de temperatura (calefactor + ventilador),
  horno remoto, equipo de sonido con volumen analógico y lista de mercado persistente.

El enunciado oficial está en [`doc/Proyecto_2026-1.md`](doc/Proyecto_2026-1.md). La traducción a
requisitos verificables está en [`specs/001-requisitos.md`](specs/001-requisitos.md).

---

## 2. Estructura del repositorio

```
ProyectoDomotico.ino     # setup() + loop() + parser de comandos serial (orquestador)
src/<modulo>.cpp          # implementación de cada módulo (un .cpp por subsistema)
include/<modulo>.h       # APIs públicas de cada módulo (expone solo lo necesario)
doc/                     # documentación de usuario y diagramas
  pinesUtilizados.md     #   mapa de pines (diagrama mermaid)
  Manual_ProyectoDomotico.md  #   manual de usuario / pruebas en Proteus
  MAPEO_PINES.md         #   tabla de pines detallada
  COMANDOS_TECLADO.md    #   catálogo de comandos por teclado
  Proyecto_2026-1.md     #   enunciado oficial del proyecto
build/                   # artefactos de compilación (.hex para Proteus) — NO editar a mano
AGENTS.md                # este archivo (punto de entrada para agentes)
README.md                # descripción general del proyecto
specs/                   # especificaciones SDD (fuente de verdad del diseño)
```

Módulos: `lcd`, `teclado`, `usart`, `alarma`, `motor`, `temperatura`, `dimmer`,
`horno`, `sonido`, `mercado` (implementados) · `spi_master`, `rfid`, `eeprom_mgr`,
`acceso` (stubs — Fase 4).

---

## 3. Cómo construir y probar

**No hay toolchain en este entorno; la compilación la hace el usuario.**

- **Arduino IDE**: abrir `ProyectoDomotico.ino` (toma los `.cpp/.h` del directorio
  automáticamente). Placa: *Arduino Mega or Mega 2560*, procesador *ATmega2560*.
  `Sketch → Export Compiled Binary` genera el `.hex` en `build/arduino.avr.mega/`.
- **arduino-cli**:
  `arduino-cli compile --fqbn arduino:avr:mega:cpu=atmega2560 .`
- **Simulación**: cargar el `.hex` en el ATmega2560 de **Proteus** (clock 16 MHz).
  El plan de pruebas manual está en `doc/Manual_ProyectoDomotico.md` §7.
- **Serial**: 9600 bps, 8N1. Terminal virtual con TX/RX **cruzados**.

No existen pruebas automatizadas (es firmware sobre hardware simulado). La verificación
es: (1) que compile sin warnings de pines, (2) las pruebas manuales en Proteus.

---

## 4. Convenciones de código (resumen — ver constitution.md)

- **Sin librerías de Arduino ni `digitalWrite`/`Serial`**. Solo registros AVR
  (`DDRx`, `PORTx`, `PINx`, timers, ADC, USART) vía `<avr/io.h>`, `<avr/interrupt.h>`.
  Excepción permitida: `<avr/eeprom.h>`, `<string.h>`, `<ctype.h>`, `<util/delay.h>`.
- **Un módulo = un par `.cpp/.h`.** El `.h` declara solo la API pública; el estado
  interno es `static` en el `.cpp`. Patrón de API: `modulo_init()`,
  `modulo_actualizar()` (si es periódico), getters.
- **Nombres y comentarios en español.** Comentarios densos explicando el *porqué* de
  cada acceso a registro (estilo ya presente en el repo — mantenerlo).
- **`modulo_init()` configura el hardware; se llama en `setup()` ANTES de `sei()`.**
- **El `loop()` es cooperativo**: nada debe bloquear largo rato salvo el motor del
  garaje (bloqueante por diseño; las ISR siguen corriendo durante su `_delay_ms`).
- **Macros de pin en el `.h`** (`#define HORNO_PORT PORTH`) y **usarlas en el `.cpp`**
  — no escribir el registro literal (ver DEF-001, causado precisamente por no hacerlo).

---

## 5. Mapa de pines / timers / interrupciones (rápido)

| Recurso | Dueño | Pines |
|---|---|---|
| Puerto A | LCD | RS=PA0, EN=PA2, D4–D7=PA4–PA7 |
| Puerto C (in) + L (out) | Teclado 4×4 | filas PC0–PC3, columnas PL0–PL3 |
| Puerto E0/E1 | USART0 | RX0=PE0, TX0=PE1 |
| INT2/INT3 (PD2/PD3) | Alarma acceso | SW1 puerta, SW2 ventana |
| INT4/INT5 (PE4/PE5) | Alarma incendio | SW3, SW4 |
| PB6/PB7 | LEDs alarma | verde=armada, rojo=disparada |
| D6 / PH3 (OC4A, Timer4) | Servo garaje | Proteus: servo; entrenador: LED |
| ADC9 (A9) + PK3/PK4 | Temperatura | pot, calefactor, ventilador (on/off) |
| ADC13 (A13/PK5) | Volumen sonido | potenciómetro de volumen |
| Timer1 / OC1A (PB5/D11) | Dimmer PWM | LED iluminación (curva log) |
| Timer2 | Teclado | escaneo (CTC) |
| Timer3 / OC3A (PE3/D5) | Sonido | PWM volumen → RC; LED equipo en PH6 (D9) |
| Horno | LED rele | PH5 (D8) — corregido (DEF-001) |

> Mapa completo y actualizado en **`doc/MAPEO_PINES.md`**. PG0–PG3 quedaron libres al
> pasar el garaje a servo.

ISRs: `INT2/INT3/INT4/INT5_vect` (alarma), `TIMER2_COMPA_vect` (teclado),
`USART0_RX_vect` (serial). **Fase 4 añadirá SPI + RFID; respetar pines de SS/MOSI/MISO/SCK.**

---

## 6. Protocolo serial (estado actual)

Comando terminado en `\n`. Respuestas `OK:...` / `ERROR:...`.

```
ARM:1234 | DISARM:1234 | LUZ:0-10 | GARAJE:ABRIR | GARAJE:CERRAR
HORNO:temp,min | SONIDO:ON,vol | SONIDO:OFF
MERCADO:ADD,nombre,cant | MERCADO:DEL,nombre | MERCADO:LIST
```
(Armar y desarmar requieren código — RF-04.)

**Teclado (entrenador sin terminal)**: cada función tiene secuencia `* ‹código2díg›
‹params› #` (`A` separa params). Ej.: sonido 75% = `*2175#`, horno 180°C/25min =
`*31180A25#`, garaje abrir = `*11#`. Tablas completas en `doc/COMANDOS_TECLADO.md`.

Alertas automáticas: `ALARMA:INCENDIO`, `ALARMA:ACCESO`, `HORNO:FIN`.
El parser está en `procesar_comando_serial()` dentro del `.ino`. Todo comando nuevo se
añade ahí, en mayúsculas (la entrada se normaliza con `a_mayusculas`).

---

## 7. Estado del proyecto (junio 2026)

- ✅ **Completo**: Fases 1 (LCD), 2 (Teclado), 3 (USART), 5 (Alarmas), 6 (Motor +
  Temperatura + Dimmer), 7 (Horno + Sonido + Mercado).
- ⏳ **Pendiente**: **Fase 4** — RFID (RC522 por SPI), gestor de EEPROM para UIDs,
  lógica de acceso (adulto/hijo), puerta principal por imán, contadores de sala de
  juegos. Hoy son **stubs vacíos** (`rfid.cpp`, `spi_master.cpp`, `eeprom_mgr.cpp`,
  `acceso.cpp`) y sus `#include` están comentados en el `.ino`.
- 🐞 **Defectos**: ver [`specs/defects/`](specs/defects/).
  - **DEF-001 (alta)**: ✅ **corregido** — `horno.cpp` ya maneja **PH5** (no PE5), sin
    colisión con el sensor de incendio SW4/INT5. Verificar al compilar.
  - **DEF-002 (media)**: 🟡 parcial — **decisión: garaje con motor paso a paso** (se
    mantiene `motor.cpp`). Falta implementar la **puerta principal por imán** en Fase 4.
  - **DEF-003 (baja)**: ✅ **corregido** — comentario de `motor.h` actualizado.

Detalle completo y matriz de trazabilidad en
[`specs/002-estado-y-roadmap.md`](specs/002-estado-y-roadmap.md).

---

## 8. Flujo de trabajo SDD para agentes

1. **Lee la spec antes de tocar código**: `specs/constitution.md` (reglas globales) y la
   spec del módulo/fase en la que trabajarás (`specs/features/` o
   `specs/fase-04-rfid-acceso/`).
2. **Una tarea = un cambio acotado.** Para Fase 4 sigue el orden de
   `specs/fase-04-rfid-acceso/tasks.md` (SPI → EEPROM → RFID → acceso → integración).
3. **Antes de añadir un pin/timer**, valida contra la tabla de §5 y
   `specs/003-arquitectura.md` que no haya colisión.
4. **Actualiza la documentación junto con el código**: si cambias una API, actualiza su
   spec en `specs/features/` y el estado en `002-estado-y-roadmap.md`.
5. **Cierra un defecto** moviendo su nota a "resuelto" en el archivo de
   `specs/defects/` y registrando el commit.
6. **Mensajes de commit**: `feat:`, `fix:`, `docs:`, `refactor:` (se sigue
   *conventional commits*, ver historial de git).
