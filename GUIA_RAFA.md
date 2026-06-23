# Guía rápida para entender el Proyecto Domótico

> Para Rafa, que retoma el proyecto para depurarlo, mejorarlo y montarlo físico.
> Esto es un mapa mental del sistema: **qué hace, cómo está organizado y por dónde
> corren los datos**. Para el detalle fino, los docs originales siguen valiendo
> (`doc/Manual_ProyectoDomotico.md`, `doc/COMANDOS_TECLADO.md`, `specs/`).

---

## 1. Qué es el proyecto en una frase

Un **controlador de casa domótica** sobre **Arduino Mega 2560 (ATmega2560)**, escrito
**sin librerías de Arduino**: todo es acceso directo a registros (`DDRx`, `PORTx`, timers,
interrupciones). Maneja **seguridad** (alarmas de acceso e incendio, acceso por tarjeta
RFID) y **confort** (luz dimerizada, temperatura, garaje, horno, equipo de sonido, lista
de mercado). Se opera por **teclado 4×4 + LCD** o por **terminal serial**.

La regla de oro del proyecto (su "constitución"): **registros directos, arquitectura
modular, nada que bloquee la seguridad**.

---

## 2. La idea clave que tienes que entender primero

Todo el firmware es **un solo bucle cooperativo**. No hay sistema operativo ni hilos.
Funciona así:

1. **`setup()`** se ejecuta UNA vez: inicializa cada módulo y al final habilita las
   interrupciones (`sei()`).
2. **`loop()`** se repite infinitamente y, en cada vuelta, revisa todos los subsistemas
   en orden de prioridad y atiende lo que haya pendiente.
3. Los **eventos urgentes** (sensores, tecla pulsada, byte que llega por serial) NO se
   esperan dentro del loop. Los capturan **ISRs** (rutinas de interrupción) cortas que
   solo **dejan una bandera o un dato**, y el `loop()` las consume cuando le toca.

Ese patrón —**ISR pone bandera, loop la lee**— es el corazón del diseño. Si lo entiendes,
entiendes el 80% del proyecto.

```
   ISRs (asíncronas, "interrumpen" lo que esté pasando)
   INT2/INT3 acceso · INT4/INT5 incendio · Timer2 teclado · USART0_RX serial
        │  dejan banderas / datos en buffers
        ▼
   loop()  ── lee banderas en orden de prioridad ──►  actúa sobre las salidas
   1.alarmas → 2.RFID → 3.teclado → 4.serial → 5.sonido → 6.temperatura/horno
        │
        ▼
   Salidas: LCD · LEDs alarma · calefactor/ventilador · dimmer · servo garaje · horno · sonido
```

---

## 3. Arquitectura de archivos (un módulo por subsistema)

Cada subsistema es un par `.cpp` (implementación) + `.h` (la "API pública", lo único que
los demás pueden usar). El `.ino` es el **director de orquesta**: incluye todos los
módulos, los inicializa y enruta los comandos. **No metas lógica de hardware en el `.ino`**;
va en su módulo.

| Módulo | Archivos | De qué se encarga |
|--------|----------|-------------------|
| Principal | `ProyectoDomotico.ino` | setup/loop, parser de teclado y serial, pega todo |
| LCD | `lcd.*` | Pantalla 16×2 HD44780 en modo 4 bits |
| Teclado | `teclado.*` | Escaneo del teclado 4×4 por interrupción de Timer2 |
| Serial | `usart.*` | USART0 a 9600 8N1, recepción por ISR a buffer circular |
| Alarma | `alarma.*` | Máquina de estados: acceso + incendio + anti-intrusos |
| ADC | `adc.*` | ADC compartido: lee temperatura (A9) y volumen (A13) |
| Temperatura | `temperatura.*` | Lee el pot, controla calefactor/ventilador con histéresis |
| Dimmer | `dimmer.*` | Luz regulable por PWM (Timer1), escala 0–10 |
| Motor/Garaje | `motor.*` | Servomotor del garaje por PWM (Timer4) |
| Horno | `horno.*` | "Relé" del horno (LED) + cuenta regresiva de minutos |
| Sonido | `sonido.*` | Volumen por PWM+filtro RC (Timer3), pot en tiempo real |
| Mercado | `mercado.*` | Lista de compras persistente en EEPROM |
| SPI | `spi_master.*` | Bus SPI maestro para el lector RFID |
| RFID | `rfid.*` | Lee tarjetas RC522 (UID + contador en la tarjeta) |
| EEPROM mgr | `eeprom_mgr.*` | Guarda UIDs autorizados y el código de alarma |
| Acceso | `acceso.*` | Decide adulto/hijo/desconocido, abre imán o garaje |

> Truco para leer un módulo: **empieza por el `.h`**. Ahí están los comentarios de qué
> hace cada función pública. Solo bajas al `.cpp` cuando necesitas el "cómo".

---

## 4. El flujo de `loop()` paso a paso

Esto es literalmente lo que pasa en cada vuelta (en este orden, que es deliberado):

1. **Alarmas** — `alarma_actualizar()`. Si una ISR dejó una bandera de disparo, muestra
   `!! INCENDIO !!` o `!! INTRUSION !!` en el LCD y avisa por serial. La seguridad va
   primero, siempre.
2. **RFID** — `rfid_verificar()`. Mira si hay una tarjeta presentada y resuelve el acceso.
3. **Teclado** — si `teclado_hay()` dice que hay una tecla nueva, la lee y la manda a
   `procesar_tecla_alarma()` (el intérprete de teclas).
4. **Serial** — si llegó una línea completa (`usart_hay_linea()`), la pasa a mayúsculas y
   la manda a `procesar_comando_serial()`.
5. **Sonido** — `sonido_actualizar()`: si el equipo está encendido, lee el potenciómetro
   de volumen y ajusta el PWM en tiempo real.
6. **Temperatura y horno** — cada `INTERVALO_TEMP` (≈2000 vueltas) lee el sensor y controla
   calefactor/ventilador; y revisa si el horno terminó su cuenta regresiva.

Nada de esto usa `delay()` largos en el flujo normal, justamente para no congelar la
seguridad. (El único punto histórico de bloqueo, el listado de mercado con `delay`, ya se
movió a serial no bloqueante.)

### Detalle bonito: cómo el teclado convive con el LCD
La línea 1 del LCD muestra temperatura (izquierda) y luz (derecha). Pero cuando estás
**digitando un código**, ese mensaje necesita TODA la línea. La solución es un contador,
`ultima_interaccion`: cada tecla lo pone a 0, y la temperatura solo vuelve a escribir en
el LCD cuando pasaron suficientes vueltas sin que toques nada. Por eso el código no se
"borra solo" mientras escribes.

---

## 5. Las dos formas de dar órdenes

El sistema acepta los mismos comandos por **dos caminos** que terminan llamando a las
**mismas funciones de módulo**. Esto es importante: si arreglas un bug en `dimmer_set()`,
se arregla para teclado y para serial a la vez.

### 5.1 Por SERIAL (terminal / Virtual Terminal de Proteus, 9600 8N1)
Escribes texto y Enter. Los principales:

| Comando | Qué hace |
|---------|----------|
| `ARM:1234` / `DISARM:1234` | Arma / desarma la alarma de acceso (pide código) |
| `LUZ:0..10` | Fija el nivel del dimmer |
| `GARAJE:ABRIR` / `GARAJE:CERRAR` | Mueve el servo del garaje |
| `HORNO:180,25` | Enciende el horno a 180° por 25 min |
| `SONIDO:ON,7` / `SONIDO:OFF` | Equipo de sonido encendido a vol 7 / apagado |
| `VOL:+` `VOL:-` `VOL:5` | Sube / baja / fija volumen (con el equipo encendido) |
| `MERCADO:ADD,leche,2` / `MERCADO:DEL,leche` / `MERCADO:LIST` | Lista de compras |
| `ENROL:ADULTO,1234` / `ENROL:HIJO,5,1234` | Registra la próxima tarjeta (adulto / hijo con 5 cupos) |
| `BORRAR,1234` | Borra la próxima tarjeta presentada |
| `ACCESOS:10,1234` | Recarga 10 accesos a la tarjeta de un hijo |

El sistema responde con `OK:...` o `ERROR:...`, y manda alertas solas como
`ALARMA:INCENDIO`, `ALARMA:ACCESO`, `ALARMA:INTRUSO`, `HORNO:FIN`, `ACCESO:CONCEDIDO_*`.

### 5.2 Por TECLADO (el entrenador físico no tiene terminal)
Hay dos modos:

**Teclas directas (modo normal):**
- `0–9` → digitas el código de alarma
- `A` → armar · `B` → desarmar (ambas validan el código de 4 dígitos)
- `C` → subir luz · `D` → bajar luz

**Modo comando** (para todo lo demás): pulsas `*`, tecleas un **código de función de 2
dígitos** + parámetros (la tecla `A` separa parámetros) y ejecutas con `#`. `*` también
cancela.

| Código | Función | Ejemplo |
|--------|---------|---------|
| `11` / `10` | garaje abrir / cerrar | `*11#` |
| `21` / `20` | sonido ON (nivel 0–10) / OFF | `*217#` → ON nivel 7 |
| `22` / `23` | volumen + / – | `*22#` |
| `31` / `30` | horno ON (temp **A** min) / OFF | `*31180A25#` → 180°, 25 min |
| `41` | luz a nivel | `*417#` → luz 7 |
| `51` / `50` / `59` | mercado agregar / quitar / listar | `*5112#` → producto 1 cant 2 |
| `60` `61` `62` | RFID borrar / enrolar adulto / enrolar hijo | `*611234#` |
| `90` / `91` | alarma desarmar / armar (con código) | `*911234#` |

> El catálogo de productos del mercado (qué número es cada producto) y todos los detalles
> están en `doc/COMANDOS_TECLADO.md`. Léelo antes de probar el mercado por teclado.

---

## 6. Subsistemas clave explicados sin tecnicismos

**Alarmas.** Son dos cosas separadas:
- *Incendio* (sensores SW3/SW4 → INT4/INT5): **siempre vigilando**, no se arma ni se
  desarma, máxima prioridad.
- *Acceso* (sensores SW1/SW2 → INT2/INT3): solo dispara si está **ARMADA**. Se arma/desarma
  con el código de 4 dígitos.
- *Anti-intrusos*: si fallas el código **3 veces seguidas** (`ALARMA_MAX_INTENTOS`),
  se dispara como intrusión. Un código correcto reinicia el contador.

**Temperatura.** Lee un potenciómetro (que simula el sensor) por el ADC, y con
**histéresis** prende calefactor (LED rojo) o ventilador (LED azul). La histéresis evita
que los LEDs parpadeen sin parar cerca del umbral.

**Dimmer (luz).** PWM por Timer1. Escala 0–10 a propósito (limitación visual confirmada en
Proteus, ver `DEF-004`).

**Garaje.** Es un **servomotor** por PWM en Timer4 (antes era motor paso a paso; se cambió
y eso liberó pines). Es **no bloqueante**: solo fija el ancho de pulso.

**Sonido.** El volumen sale como señal analógica de verdad: PWM (Timer3) + filtro RC. El
nivel lo da un potenciómetro leído en tiempo real, o un comando.

**RFID + acceso.** El lector RC522 va por SPI. Cada tarjeta tiene un UID; los autorizados
se guardan en EEPROM (hasta 10). Una tarjeta puede ser **adulto** (acceso libre) o **hijo**
(con un contador de "cupos" guardado en la propia tarjeta, que se descuenta en cada
ingreso a la sala de juegos; los padres lo recargan con `ACCESOS:`).

**Persistencia (EEPROM).** Lo que debe sobrevivir a un apagón se guarda en EEPROM con
zonas que NO se deben solapar:

| Rango EEPROM | Contenido |
|--------------|-----------|
| `0x000–0x09F` | Hasta 10 UIDs autorizados (4 bytes UID + 1 tipo) |
| `0x0A0–0x0A3` | Código de alarma |
| `0x0B0–0x1FF` | Lista de mercado (10 ítems × 14 bytes) |

---

## 7. Mapa de pines (referencia para el montaje)

Esta es la tabla autoritativa (de `specs/003-arquitectura.md`). Útil cuando pases de
Proteus al entrenador físico.

| Pin Mega | Puerto | Dir | Función | Módulo |
|----------|--------|-----|---------|--------|
| D22 | PA0 | OUT | LCD RS | lcd |
| D24 | PA2 | OUT | LCD EN | lcd |
| D26–D29 | PA4–PA7 | OUT | LCD D4–D7 | lcd |
| D37–D34 | PC0–PC3 | IN pull-up | Teclado filas 0–3 | teclado |
| D49–D46 | PL0–PL3 | OUT | Teclado columnas 0–3 | teclado |
| D0 / D1 | PE0 / PE1 | IN/OUT | USART0 RX / TX | usart |
| D19 / D18 | PD2 / PD3 | IN | INT2/INT3 — acceso (puerta/ventana) | alarma |
| D2 / D3 | PE4 / PE5 | IN | INT4/INT5 — incendio (zona 1/2) | alarma |
| D12 / D13 | PB6 / PB7 | OUT | LED verde armada / LED rojo disparada | alarma |
| D6 | PH3 | PWM | Servo garaje (Timer4, 50 Hz) | motor |
| A9 | PK1 (ADC9) | IN | Potenciómetro temperatura | temperatura/adc |
| A13 | PK5 (ADC13) | IN | Potenciómetro volumen | sonido/adc |
| A11 / A12 | PK3 / PK4 | OUT | Calefactor / Ventilador | temperatura |
| D11 | PB5 | PWM | Dimmer (Timer1) | dimmer |
| D5 | PE3 | PWM | Volumen sonido (Timer3) → filtro RC | sonido |
| D9 | PH6 | OUT | LED equipo de sonido | sonido |
| D8 | PH5 | OUT | LED horno | horno |
| D41 | PG0 | OUT | Imán puerta principal | acceso |
| D53/D52/D51/D50 | PB0/PB1/PB2/PB3 | — | SPI: CS / SCK / MOSI / MISO (RC522) | spi_master |

**Timers:** Timer1=dimmer, Timer2=teclado, Timer3=sonido, Timer4=servo garaje.
Timer0 y Timer5 libres.

> Ojo Proteus vs físico: en Proteus el garaje es un servo y el dimmer/horno/calefactor son
> LEDs con R220; en el entrenador real algunas salidas son LED + resistencia. La columna
> "Notas" de `doc/MAPEO_PINES.md` lo aclara señal por señal.

---

## 8. Cómo probarlo (plan rápido)

Antes de cualquier cosa: **compila** (Arduino IDE o `arduino-cli`) y carga el `.hex` en
Proteus. (Los autores dejaron escrito que NO pudieron compilar en su entorno; lo suyo fue
análisis estático, así que la primera compilación limpia es tarea pendiente real.)

Checklist de humo (de menor a mayor riesgo):

1. **Arranque:** el LCD debe mostrar `ALARMA: DESACTIV` arriba y `T:--C   L:0/10` abajo, y
   la terminal debe imprimir el menú de comandos.
2. **Luz:** `LUZ:7` por serial y `C`/`D` por teclado → debe verse `L:7/10` y cambiar el LED.
3. **Temperatura:** mueve el pot A9 → calefactor/ventilador (LEDs A11/A12) deben conmutar
   con histéresis, sin parpadeo.
4. **Alarma acceso:** teclea código + `A` (armar), dispara SW1/SW2 → `!! INTRUSION !!` +
   `ALARMA:ACCESO`. Desarma con código + `B`.
5. **Anti-intrusos:** teclea 3 códigos malos seguidos → `ALARMA:INTRUSO`.
6. **Incendio:** dispara SW3/SW4 → `!! INCENDIO !!` aunque la alarma esté desarmada.
7. **Garaje, horno, sonido, mercado:** uno por uno con los comandos de la sección 5.
8. **RFID:** enrola una tarjeta (`ENROL:ADULTO,1234`), preséntala → `ACCESO:CONCEDIDO_*`.
   Prueba un hijo con cupos y que el contador baje.

**Punto a verificar con lupa:** la **alarma de incendio zona 2** (SW4/INT5). Hubo un bug
(`DEF-001`) en que el horno usaba el mismo pin PE5 que ese sensor; ya está corregido en el
código, pero conviene confirmar en simulación que zona 2 dispara bien.

---

## 9. Dónde mejorar / a qué ponerle ojo al depurar

Cosas reales que vale la pena revisar (no son fallos confirmados, son los puntos frágiles
del diseño):

- **Compilación pendiente.** Nadie compiló en el entorno de los autores. La primera tarea
  honesta es compilar limpio y resolver warnings. Empieza por ahí.
- **Base de tiempo imprecisa.** La temperatura y el "tiempo sin interacción" se miden
  **contando vueltas del loop**, no con un timer real. Si agregas código pesado al loop,
  esos tiempos (≈2-3 s) se alargan. Si quieres precisión, migrar a un timer dedicado
  (Timer0/Timer5 están libres) sería una buena mejora.
- **Movimientos bloqueantes.** Revisa `garaje_abrir()`/`garaje_cerrar()`: el comentario del
  `.ino` los describe como bloqueantes (~2.5 s) en algunos caminos, pero `motor.*` ya es
  servo no bloqueante. Confirma cuál versión está activa para que no congele el loop.
- **Buffers de tamaño fijo.** `comando[32]`, `cmd_buffer[16]`, etc. Comandos muy largos
  (nombres de mercado largos) se truncan en silencio. Vale validar longitudes.
- **`texto_a_numero()` satura en 255** (es `uint8_t`). Bien para luz/volumen/temp, pero
  tenlo presente si reusas la función para algo más grande.
- **Zonas de EEPROM.** Si tocas mercado o RFID, respeta los rangos de la sección 6 para no
  pisar el código de alarma o los UIDs.
- **Verificar incendio zona 2** tras el fix de `DEF-001` (lo mismo de la sección 8).
- **README vacío.** `README.md` del repo está en blanco; sería buen lugar para enlazar esta
  guía y los docs de `specs/`.

---

## 10. Por dónde empezar a leer (ruta sugerida)

1. Esta guía (ya la estás leyendo 🙂).
2. `ProyectoDomotico.ino` → lee solo `setup()` y `loop()` (final del archivo). Ahí ves el
   esqueleto completo.
3. `specs/003-arquitectura.md` → el diagrama y el mapa de pines autoritativo.
4. `doc/COMANDOS_TECLADO.md` → para poder operar el entrenador.
5. Un módulo a tu elección, empezando por su `.h`. Sugerencia: `alarma.h` + `alarma.cpp`,
   que es el más representativo del patrón ISR→bandera→loop.

---

*Documento de apoyo creado para retomar el proyecto. No reemplaza al `Manual_ProyectoDomotico.md`
ni a los `specs/`; los resume y los conecta.*
