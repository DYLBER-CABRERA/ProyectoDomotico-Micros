# AGENTS.md — Guía maestra para agentes de IA

> Sistema domótico sobre **ATmega2560 (Arduino Mega 2560)**, en **registros directos del
> AVR, SIN librerías de Arduino**. Proyecto de Microprocesadores 2026-1.
> Autores: **Dylber Cabrera**, **Juan David Ocampo**, **Alejandro Martínez**.
>
> **Este es el punto de entrada para cualquier agente (Claude, Copilot, etc.).** Léelo
> COMPLETO antes de tocar código. El proyecto YA FUE SUSTENTADO con éxito y FUNCIONA en el
> entrenador físico; las próximas sustentaciones son de **modificación de código**.
>
> ⚠️ **La profesora califica solo "funciona / no funciona" (0/5).** La regla #1 es:
> **NO ROMPER lo que ya funciona.** Haz cambios pequeños y acotados, respeta los patrones
> existentes, y deja que el usuario compile y pruebe en hardware antes de dar por hecho nada.

---

## 0. Lo que NO puedes hacer aquí (y cómo se compila)

- **No hay toolchain AVR en este entorno**: no puedes compilar ni subir. **El usuario
  compila y sube** desde su PC (Arduino IDE 2.x) por **COM13** (a veces con `avrdude`).
- Los archivos del proyecto están en **OneDrive**: a veces `bash` no los ve (cloud-only).
  Usa las herramientas de archivo (Read/Edit/Write) que sí los hidratan. No dependas de
  `git`/`grep` por bash dentro del repo; usa Grep/Read.
- **Verifica tus ediciones releyendo** el bloque editado. No asumas que compiló.
- La carpeta `build/` y los `.hex`/`.elf` están en `.gitignore` (son artefactos; NO se
  versionan — causaban conflictos de merge binarios).

Compilar/subir (lo hace el usuario):
- Arduino IDE: abrir `ProyectoDomotico.ino` (toma `src/` e `include/` solo). Placa
  *Arduino Mega or Mega 2560*, procesador *ATmega2560*.
- `Sketch → Export Compiled Binary` deja el `.hex` en `build/arduino.avr.mega/`.
- Subir por consola: `avrdude -c wiring -p atmega2560 -P COM13 -b 115200 -D -U flash:w:"...ino.hex":i`

---

## 1. Arquitectura — la idea central

Firmware de **un solo bucle cooperativo**. No hay RTOS ni hilos.

```
ISRs (asíncronas)  →  dejan BANDERAS/datos  →  loop() las consume por prioridad
INT2/3 acceso · INT4/5 incendio · TIMER2 (teclado + reloj ms) · USART0_RX (serial)
        │
   setup(): cada *_init(); luego sei()
        │
   loop(): 1.alarmas → 1b.RFID → 1c.pulsadores → 1d.retardo desarme
           → 2.teclado → 3.serial → 3b.sonido → 3c.marquee mercado
           → 4.temperatura+horno → 5.auto-recuperación LCD
        │
   Salidas: LCD · LEDs alarma · calefactor/ventilador · dimmer · servo · imán · horno · sonido
```

**Patrón clave: ISR pone bandera → loop la lee.** Las ISR son MÍNIMAS (solo banderas;
nunca LCD/USART dentro de una ISR).

### Base de tiempo REAL — `millis_sistema()` (¡importante!)
Para medir tiempo se usa **`millis_sistema()`** (en `teclado.cpp`), un reloj en
milisegundos que avanza en la ISR del **Timer2** (`ms_sistema += 10` cada ~10 ms).
- ❌ **NUNCA midas tiempo contando "vueltas del loop".** El loop tiene velocidad
  variable (el sondeo RFID lo frena), así que contar vueltas da tiempos erráticos.
- ✅ Para temporizar algo: guarda `t0 = millis_sistema()` y compara
  `millis_sistema() - t0 >= MILISEGUNDOS`. Así lo hacen horno, temperatura, retardo de
  desarme, antirrebote de pulsadores y auto-recuperación del LCD.

---

## 2. Estructura del repositorio y función de cada archivo

```
ProyectoDomotico.ino   # Orquestador: setup(), loop(), parser serial y de teclado,
                       #   selector de lugar (pulsadores), retardo de desarme, marquee.
src/  +  include/      # un par .cpp/.h por subsistema (el .h = API pública; estado static en .cpp)
doc/                   # documentación de usuario (COMANDOS, MAPEO_PINES, Manual, enunciado)
specs/                 # SDD: constitution, requisitos, arquitectura, estado/roadmap, defects
GUIA_RAFA.md           # explicación didáctica del proyecto (flujos, módulos) — buen resumen
build/                 # .hex/.elf (artefactos, en .gitignore — NO editar)
```

**Función de cada módulo** (todos: `modulo_init()` + API):

| Archivo | Qué hace | Funciones/notas clave |
|---------|----------|------------------------|
| `lcd.*` | LCD 16×2 HD44780, modo 4 bits, Puerto A | `lcd_init`, `lcd_goto`, `lcd_string`, `lcd_int`, `lcd_clear`, **`lcd_resync`** (recupera el LCD si el ruido lo corrompe, sin borrar) |
| `teclado.*` | Teclado 4×4 (escaneo por Timer2) **+ reloj `millis_sistema()`** | debounce por integración (`TEC_MUESTRAS_ESTABLE`); `teclado_hay/leer`; `millis_sistema` |
| `usart.*` | USART0 9600 8N1, RX por ISR a buffer circular | `usart_enviar_string/int/newline` (TX bloqueante), `usart_hay_linea`, `usart_leer_linea` |
| `alarma.*` | Máquina de estados: acceso + incendio + anti-intrusos | `alarma_armar/desarmar/actualizar/estado/tipo_disparo/verificar_codigo`, **`alarma_disparar_intruso`** (lo usa el retardo de desarme) |
| `adc.*` | ADC compartido (mux conmutado, descarte + promedio) | `adc_leer(canal)`; maneja MUX5 para A8–A15 |
| `temperatura.*` | Lee pot (ADC9) + histéresis calefactor/ventilador | `temp_celsius`, `temp_controlar` (PK3/PK4) |
| `dimmer.*` | Luz PWM (Timer1), escala 0–10 | `dimmer_set/get` |
| `motor.*` | Servo del garaje (Timer4, PWM 50 Hz, no bloqueante) | `garaje_abrir/cerrar` |
| `horno.*` | "Relé" del horno (LED PH5) + cuenta regresiva en **SEGUNDOS** | `horno_encender(temp,segundos)`, `horno_actualizar` (usa millis) |
| `sonido.*` | Volumen por PWM (Timer3) + LED; nivel por comando/pot | `sonido_encender/apagar/set_volumen/estado/volumen` |
| `mercado.*` | Lista de compras persistente en EEPROM | `mercado_agregar` (**reemplaza** cantidad si existe → retorna 2), `mercado_eliminar`, `mercado_listar` |
| `spi_master.*` | SPI maestro por hardware (Puerto B) para el RC522 | `spi_init`, `spi_transferir`, `spi_cs_bajar/subir` |
| `rfid.*` | Lector RC522 (REQA, anticolisión, SELECT, auth MIFARE, READ/WRITE bloque) | `rfid_init`, `rfid_verificar` (máquina de estados en loop), enrolar/borrar/recargar |
| `eeprom_mgr.*` | UIDs autorizados + código en EEPROM interna | `eeprom_guardar_uid/buscar_uid/borrar_uid/leer` |
| `acceso.*` | Imán puerta (PG0) + garaje; concede/deniega | `acceso_concedido_adulto/hijo`, `acceso_abrir_garaje`, `acceso_denegado` |

---

## 3. Mapa de pines / timers / interrupciones (AUTORITATIVO)

| Recurso | Pin Mega | Puerto | Uso |
|---|---|---|---|
| LCD RS/EN/D4–D7 | D22/D24/D26–D29 | PA0/PA2/PA4–PA7 | pantalla |
| Teclado filas / columnas | D37–D34 / D49–D46 | PC0–PC3 / PL0–PL3 | escaneo Timer2 |
| USART0 RX/TX | D0/D1 | PE0/PE1 | serial 9600 |
| Alarma acceso | D19/D18 | INT2/INT3 (PD2/PD3) | SW1 puerta, SW2 ventana |
| Alarma incendio | D2/D3 | INT4/INT5 (PE4/PE5) | SW3, SW4 humo |
| LEDs alarma | D12/D13 | PB6/PB7 | verde armada / rojo disparada |
| Servo garaje | D6 | PH3 (OC4A, Timer4) | PWM 50 Hz |
| Temp / calefactor / ventilador | A9 / A11 / A12 | PK1(ADC9) / PK3 / PK4 | pot + on/off |
| Volumen sonido | A13 | PK5 (ADC13) | potenciómetro |
| Dimmer | D11 | PB5 (OC1A, Timer1) | PWM luz |
| Sonido PWM / LED | D5 / D9 | PE3 (OC3A, Timer3) / PH6 | volumen + LED |
| Horno | D8 | PH5 | LED relé |
| Imán puerta principal | D41 | PG0 | acceso |
| **SPI RC522** | D53/D52/D51/D50 | PB0/PB1/PB2/PB3 | CS/SCK/MOSI/MISO |
| **RFID RST (NRSTPD)** | D4 | PG5 | reset del RC522 |
| **Pulsadores selector lugar** | D40/D39 | PG1/PG2 | izquierda/derecha |

**Timers**: Timer1=dimmer, **Timer2=teclado + reloj ms**, Timer3=sonido, Timer4=servo.
Timer0 y Timer5 libres. **ADC**: ADC9 (temp) y ADC13 (volumen).

> ⚠️ **Timer2**: en `teclado_init()` el prescaler DEBE ser **/1024** = `CS22|CS21|CS20`
> (bits `111`). En el Timer2, `101` es **/128** (≈8× más rápido) y descuadra el reloj de
> ms y el horno. Si tocas Timer2, conserva `/1024` y `OCR2A=155` (~10 ms).

---

## 4. Comandos (resumen — detalle en `doc/COMANDOS_TECLADO.md`)

- **Serial (9600 8N1):** `ARM:1234` `DISARM:1234` `LUZ:0-10` `GARAJE:ABRIR/CERRAR`
  `HORNO:temp,segundos` `SONIDO:ON[,vol]` `SONIDO:OFF` `VOL:N/+/-`
  `MERCADO:ADD,nom,cant` `MERCADO:DEL,nom` `MERCADO:LIST`
  `ENROL:ADULTO,cod` `ENROL:HIJO,n,cod` `BORRAR,cod` `ACCESOS:n,cod`.
- **Teclado:** directas `A`/`B` (armar/desarmar), `C`/`D` (luz ±), `0-9` (código).
  Modo comando `*‹FF›‹params›#`: `11/10` garaje, `21/20/22/23` sonido, `31/30` horno
  (`*31‹temp›A‹seg›#`), `41` luz, `51/50/59` mercado, `61/62/63/60` RFID (enrol adulto/
  hijo/recargar/borrar), `91/90` alarma.
- El parser serial está en `procesar_comando_serial()` y el de teclado en
  `ejecutar_comando_teclado()`, ambos en el `.ino`. **Comando nuevo = agregar un `else if`
  (serial) o un `case` (teclado) ahí.** La entrada serial se normaliza a mayúsculas.

---

## 5. Lógica de acceso (los 3 lugares + 2 pulsadores)

- **2 pulsadores** (D40 izq / D39 der) alternan el "lugar" actual en lista circular:
  **PUERTA → GARAJE → SALA**. Al pulsar, el LCD muestra `LUGAR: ...`. El estado vive en
  `lugar_actual` (`.ino`); el RFID lo lee con `obtener_lugar()`.
- Al pasar una tarjeta válida, `rfid.cpp` (`manejar_tarjeta_ok`) actúa según el lugar:
  - **PUERTA**: abre imán + (si la alarma está ARMADA) **10 s** para desarmar.
  - **GARAJE**: abre servo + (si ARMADA) **15 s** para desarmar.
  - **SALA**: **adulto** entra libre; **niño** descuenta 1 cupo del bloque MIFARE.
- **Retardo de desarme** (`acceso_iniciar_retardo` en el `.ino`): si no se desarma a
  tiempo, `alarma_disparar_intruso()` → `ALARMA:INTRUSO`. Se cancela solo al desarmar
  (cuando `alarma_estado() != ARMADA`).

---

## 6. Reglas de oro / convenciones (ver `specs/constitution.md`)

- **Sin librerías de Arduino** (`digitalWrite`/`Serial`/`delay()` de Arduino NO). Solo
  registros AVR. Permitidos: `<avr/io.h>`, `<avr/interrupt.h>`, `<avr/eeprom.h>`,
  `<util/delay.h>`, `<string.h>`, `<ctype.h>`.
- **Un módulo = un par `.cpp/.h`.** API pública en el `.h`; estado interno `static` en
  el `.cpp`. Para que un módulo use algo del `.ino`, se declara `extern` (ej.
  `obtener_lugar`, `rfid_notifica_lcd`).
- **`modulo_init()` en `setup()` ANTES de `sei()`.** Nada bloqueante largo en el `loop()`.
- **ISR mínima**: solo banderas/contadores; jamás LCD/USART dentro de una ISR.
- **Comentarios densos en español** explicando el *porqué* de cada registro (mantener el
  estilo existente).
- **LCD**: línea 0 = estado de alarma + mensajes de evento (RFID/CMD/garaje/lugar);
  línea 1 = temperatura (cols 0–8) + luz (cols 9–15). Mensajes de evento van a la
  **línea 0** para no chocar con la temperatura.

---

## 7. ⚠️ Lecciones aprendidas (landmines — NO repetir estos errores)

Cosas que YA se rompieron y se arreglaron esta sesión. Si modificas estas zonas, cuidado:

1. **Tiempo por "vueltas del loop" = MAL.** Todo lo temporizado usa `millis_sistema()`.
   El horno y la temperatura fallaban (contaban acelerado/lento) hasta migrar a ms reales.
2. **Timer2 prescaler** debe ser `/1024` (`111`), no `/128` (`101`). Ver §3.
3. **RFID — lectura de registro**: en `rc522_leer_reg` el bit de lectura va en el **bit 7**
   (`(reg<<1) | 0x80`), NO en el bit 0. Es el protocolo MFRC522.
4. **RFID — MIFARE READ/WRITE necesitan CRC** (2 bytes) al final, calculado con
   `rc522_calcular_crc`. Sin CRC la tarjeta ignora el comando (fallaba el contador).
5. **RFID — `rfid_init`** debe poner `TxASKReg=0x40` (100% ASK) y `ModeReg=0x3D` (CRC) y
   esperar el reset; sin eso la antena no lee tarjetas. RST del RC522 en **D4 (PG5)**.
6. **RFID — relectura múltiple**: hay supresión por UID + cooldown (no reprocesar la misma
   tarjeta mientras siga puesta). No la quites.
7. **LCD se corrompe con ruido** del entrenador → `lcd_resync()` lo recupera. El auto-
   refresco en reposo (sección 5 del loop) lo repinta solo. **NO** repintes la línea 1
   (zona de luz) en cada vuelta sin re-sincronizar: corrompía el LCD.
8. **Pulsadores**: con el entrenador, déjalos con un nivel de reposo DEFINIDO. Si quedan
   "flotando" (entrada sin pull-up ni nivel externo) escriben basura en el LCD sin parar.
   Si no responden, revisar si van a GND (usar pull-up + activo-bajo) o a VCC (necesitan
   pull-down externo + activo-alto). **Probar SIEMPRE en hardware tras tocarlos.**
9. **Macros de pin del `.h`, no registros literales** (DEF-001: el horno escribía PE5 y
   tumbaba la alarma de incendio; ahora usa `HORNO_PORT`/`HORNO_PIN` = PH5).

---

## 8. Flujo de trabajo SDD para modificar (sin romper nada)

1. **Lee primero**: este AGENTS.md completo + `specs/constitution.md` + la spec de la
   zona que vas a tocar (`specs/`). Mira también la sección 7 (landmines).
2. **Cambio acotado**: una funcionalidad a la vez. Reutiliza los patrones existentes
   (un nuevo comando = un `case`/`else if` en el parser; un nuevo periódico = un timer con
   `millis_sistema()`).
3. **Antes de usar un pin/timer nuevo**, valida contra §3 que esté libre (Timer0/Timer5
   libres; pines libres documentados en `doc/MAPEO_PINES.md`).
4. **No toques** lo que ya funciona si tu cambio no lo necesita. Cambios pequeños y
   localizados; preserva el comportamiento existente.
5. **Verifica releyendo** tu edición. **El usuario debe compilar y probar en hardware**
   antes de cerrar — la profesora califica funciona/no funciona.
6. **Actualiza la doc** junto al código: `doc/COMANDOS_TECLADO.md` (comandos),
   `specs/002-estado-y-roadmap.md` (estado), y este AGENTS.md si cambian patrones/pines.
7. **Commits**: `feat:` / `fix:` / `docs:` / `refactor:` (conventional commits).

---

## 9. Estado del proyecto (sustentado y funcionando ✅)

Todas las fases funcionan en el entrenador físico: LCD, teclado, USART, alarmas (acceso +
incendio + anti-intrusos), motor/garaje (servo), temperatura (histéresis), dimmer, horno
(en segundos), sonido, mercado (persistente, con reemplazo de cantidad), RFID + acceso
(3 lugares con 2 pulsadores, contador de la sala, retardo de desarme), y reloj de tiempo
real + auto-recuperación del LCD. Historial de defectos en `specs/defects/`.

*Sistema Domótico — Microprocesadores 2026-1 · ATmega2560 · Dylber Cabrera, Juan David Ocampo, Alejandro Martínez*
