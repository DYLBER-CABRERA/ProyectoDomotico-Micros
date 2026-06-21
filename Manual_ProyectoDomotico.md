# Manual del Sistema Domótico — ATmega2560 (Arduino Mega 2560)
## Fases Activas: 1 (LCD) · 2 (Teclado) · 3 (USART) · 5 (Alarma) · 6 (Motor + Temperatura + Dimmer)

---

## 1. Descripción General

El sistema domótico controla la seguridad de una vivienda a través de dos alarmas independientes:

| Alarma | Sensores | Activación |
|--------|----------|------------|
| **Acceso** (puertas/ventanas) | SW1 (puerta), SW2 (ventana/garaje) — INT2/INT3 | Solo cuando está ARMADA con código de 4 dígitos |
| **Incendio** (humo) | SW3 (zona 1), SW4 (zona 2) — INT4/INT5 | SIEMPRE activa desde el encendido |

Adicionalmente controla:
- **Garaje**: motor paso a paso (abrir/cerrar por comando serial)
- **Temperatura**: calefactor y ventilador con control por histéresis
- **Iluminación**: dimmer PWM, nivel 0-10 (teclado o comando serial)

---

## 2. Módulos Implementados

| Fase | Módulo | Archivo | Estado |
|------|--------|---------|--------|
| 1 | LCD 16x2 HD44780 | `lcd.cpp` | ✅ Activo |
| 2 | Teclado 4x4 | `teclado.cpp` | ✅ Activo |
| 3 | USART0 serial | `usart.cpp` | ✅ Activo |
| 4 | RFID + EEPROM + Acceso | `rfid.cpp`, `eeprom_mgr.cpp`, `acceso.cpp` | ⏳ Pendiente (stubs) |
| 5 | Alarma acceso + incendio | `alarma.cpp` | ✅ Activo |
| 6a | Motor PAP garaje | `motor.cpp` | ✅ Activo |
| 6b | Control temperatura | `temperatura.cpp` | ✅ Activo |
| 6c | Dimmer iluminación | `dimmer.cpp` | ✅ Activo |
| 7 | Horno, Sonido, Mercado | `horno.cpp`, `sonido.cpp`, `mercado.cpp` | ⏳ Pendiente (stubs) |

---

## 3. Mapa de Pines

Ver el archivo `pinesUtilizados.mermaid` para el diagrama completo. Resumen:

### Puerto A — LCD 16×2 HD44780
| Pin Mega | Puerto AVR | Dirección | Señal LCD |
|----------|-----------|-----------|-----------|
| D22 | PA0 | SALIDA | RS (Register Select) |
| D24 | PA2 | SALIDA | EN (Enable) |
| D26 | PA4 | SALIDA | D4 |
| D27 | PA5 | SALIDA | D5 |
| D28 | PA6 | SALIDA | D6 |
| D29 | PA7 | SALIDA | D7 |
> PA1 (RW) va a GND fijo. D0–D3 del LCD sin conectar (modo 4 bits).

### Puerto C / Puerto L — Teclado 4×4
| Pin Mega | Puerto AVR | Dirección | Función |
|----------|-----------|-----------|---------|
| D37 | PC0 | ENTRADA + pull-up | Fila 0 (1,2,3,A) |
| D36 | PC1 | ENTRADA + pull-up | Fila 1 (4,5,6,B) |
| D35 | PC2 | ENTRADA + pull-up | Fila 2 (7,8,9,C) |
| D34 | PC3 | ENTRADA + pull-up | Fila 3 (*,0,#,D) |
| D49 | PL0 | SALIDA | Columna 0 |
| D48 | PL1 | SALIDA | Columna 1 |
| D47 | PL2 | SALIDA | Columna 2 |
| D46 | PL3 | SALIDA | Columna 3 |

### Puerto E — USART0
| Pin Mega | Puerto AVR | Dirección | Señal |
|----------|-----------|-----------|-------|
| D0 | PE0 | ENTRADA | RX0 ← TXD de la terminal |
| D1 | PE1 | SALIDA | TX0 → RXD de la terminal |

### Puerto D / E / B — Alarma
| Pin Mega | Puerto AVR | Dirección | Función |
|----------|-----------|-----------|---------|
| D19 | PD2 | ENTRADA | INT2 — Sensor puerta (SW1) |
| D18 | PD3 | ENTRADA | INT3 — Sensor ventana (SW2) |
| D2 | PE4 | ENTRADA | INT4 — Sensor humo zona 1 (SW3) |
| D3 | PE5 | ENTRADA | INT5 — Sensor humo zona 2 (SW4) |
| D12 | PB6 | SALIDA | LED verde — alarma armada |
| D13 | PB7 | SALIDA | LED rojo — alarma disparada |
> Todos los sensores: R10kΩ a GND. LED: R220Ω en serie a GND.

### Puerto G — Motor PAP Garaje
| Pin Mega | Puerto AVR | Dirección | Función |
|----------|-----------|-----------|---------|
| D41 | PG0 | SALIDA | IN1 bobina |
| D40 | PG1 | SALIDA | IN2 bobina |
| D39 | PG2 | SALIDA | IN3 bobina |
| D38 | PG3 | SALIDA | IN4 bobina |

### Puerto K — Temperatura
| Pin Mega | Puerto AVR | Dirección | Función |
|----------|-----------|-----------|---------|
| A9 | PK1 | ENTRADA ADC9 | Potenciómetro (simula sensor) ⚠️ |
| A11 | PK3 | SALIDA | Calefactor (LED rojo) |
| A12 | PK4 | SALIDA | Ventilador (LED azul) |

> ⚠️ **Nota ADC**: El código en `temp_init()` usa `ADMUX = (1<<REFS0) | 0x01` con `MUX5=1`,
> lo que efectivamente selecciona **ADC9 (PK1, A9)** y no ADC8. Conectar el potenciómetro
> a **A9** (no A8). Para cambiar a ADC8, reemplazar `0x01` por `0x00` en `temp_init()`.

### Puerto B — Dimmer PWM
| Pin Mega | Puerto AVR | Dirección | Función |
|----------|-----------|-----------|---------|
| D11 | PB5 (OC1A) | SALIDA PWM | Dimmer iluminación (Timer1) |

---

## 4. Protocolo de Comandos Seriales

Configuración: **9600 bps, 8N1** (8 bits, sin paridad, 1 stop bit).

Enviar el comando terminado en `\n` (Enter). El sistema responde por la misma terminal.

| Comando | Acción | Respuesta OK | Respuesta ERROR |
|---------|--------|-------------|----------------|
| `ARM` | Arma la alarma de acceso | `OK:ARMADA` | — |
| `DISARM:1234` | Desarma la alarma con código | `OK:DESACTIVADA` | `ERROR:CODIGO` |
| `LUZ:0` … `LUZ:10` | Ajusta el dimmer (0=apagado, 10=máx) | `OK:LUZ,N` | — |
| `GARAJE:ABRIR` | Gira el motor PAP en sentido horario | `OK:GARAJE_ABRIENDO` → `OK:GARAJE_ABIERTO` | — |
| `GARAJE:CERRAR` | Gira el motor PAP en sentido antihorario | `OK:GARAJE_CERRANDO` → `OK:GARAJE_CERRADO` | — |
| (cualquier otro) | — | — | `ERROR:COMANDO_DESCONOCIDO` |

> El sistema también envía alertas automáticas:
> - `ALARMA:INCENDIO` cuando se activa INT4 o INT5
> - `ALARMA:ACCESO` cuando se activa INT2 o INT3 (solo si alarma armada)

---

## 5. Configuración en Proteus

### 5.1 Componentes necesarios

| Componente | Referencia Proteus | Cantidad |
|------------|-------------------|----------|
| Arduino Mega 2560 (o ATmega2560) | ARDUINO MEGA2560 | 1 |
| LCD 16×2 HD44780 | LM016L | 1 |
| Teclado matricial 4×4 | KEYPAD-PHONE o KEYPAD-SMALLCALC | 1 |
| Terminal Virtual | VIRTUAL TERMINAL | 1 |
| DIP Switch (4 posiciones) | DIPSW_4 o 4 botones individuales | 2 |
| LED rojo (alarma disparada) | LED-RED | 1 |
| LED verde (alarma armada) | LED-GREEN | 1 |
| Motor paso a paso + driver | MOTOR-STEPPER o 28BYJ-48 | 1 |
| Driver ULN2003 | ULN2003A | 1 |
| Potenciómetro 10kΩ | POT-LIN | 1 |
| LED rojo (calefactor) | LED-RED | 1 |
| LED azul (ventilador) | LED-BLUE | 1 |
| LED (dimmer) | LED-YELLOW | 1 |
| Resistencias 220Ω | RES | 2 (LEDs alarma) |
| Resistencias 10kΩ | RES | 4 (pull-down sensores) |

### 5.2 Conexión del LCD (Puerto A)

```
LCD Pin  →  Arduino Mega Pin
─────────────────────────────
VSS      →  GND
VDD      →  5V
V0       →  Potenciómetro 10kΩ (contraste) o GND para contraste fijo
RS       →  D22 (PA0)
RW       →  GND
E        →  D24 (PA2)
D0-D3    →  SIN CONECTAR (modo 4 bits)
D4       →  D26 (PA4)
D5       →  D27 (PA5)
D6       →  D28 (PA6)
D7       →  D29 (PA7)
A (LED+) →  5V a través de R100Ω (retroiluminación, opcional en Proteus)
K (LED-) →  GND
```

En Proteus: el componente `LM016L` ya tiene los pines internos conectados, solo conectar RS, E, D4-D7.

### 5.3 Conexión del Teclado (Puerto C + Puerto L)

El teclado es una matriz 4×4. Las **columnas** son manejadas por el Mega (salidas), las **filas** son leídas por el Mega (entradas con pull-up interno).

```
Teclado Pin  →  Arduino Mega Pin  →  Descripción
────────────────────────────────────────────────────
Columna 0    →  D49 (PL0)         →  Activa fila con LOW
Columna 1    →  D48 (PL1)
Columna 2    →  D47 (PL2)
Columna 3    →  D46 (PL3)
Fila 0       →  D37 (PC0)         →  Lee presencia de tecla (sin R ext.)
Fila 1       →  D36 (PC1)
Fila 2       →  D35 (PC2)
Fila 3       →  D34 (PC3)
```

> Los pines de fila NO llevan resistencias pull-down externas. El código activa el **pull-up interno** del Mega mediante `PORTC |= 0x0F`, por lo que en reposo la fila está en HIGH y al presionar una tecla cae a LOW.

En Proteus: usar el componente `KEYPAD-PHONE` o uno genérico. Asignar manualmente cada pin de fila y columna según la tabla.

### 5.4 Conexión USART0 — Virtual Terminal

```
Arduino Mega Pin  →  Virtual Terminal Pin
──────────────────────────────────────────
D1 (TX0, PE1)     →  RXD  (cables CRUZADOS)
D0 (RX0, PE0)     →  TXD  (cables CRUZADOS)
GND               →  GND
```

Configurar la Virtual Terminal en Proteus:
- Baud Rate: **9600**
- Data Bits: **8**
- Parity: **None**
- Stop Bits: **1**
- Flow Control: **None**

> En Proteus, el cable de TX del Mega va al RXD de la terminal y viceversa (cruzado). Si lo conectas directo (no cruzado) no recibirás nada.

### 5.5 Conexión Alarma (Puerto D + E + B)

**Sensores de acceso** (SW1, SW2 — activan INT2, INT3):
```
DIP-SW1 (posición 1)  →  D19 (PD2/INT2)
DIP-SW1 (posición 2)  →  D18 (PD3/INT3)
Cada switch:
  Un lado del switch   →  5V
  El otro lado         →  Pin del Mega
  R10kΩ               →  Del mismo pin a GND (pull-down)
```

**Sensores de humo** (SW3, SW4 — activan INT4, INT5):
```
DIP-SW2 (posición 1)  →  D2  (PE4/INT4)
DIP-SW2 (posición 2)  →  D3  (PE5/INT5)
Misma configuración de pull-down R10kΩ a GND.
```

**LEDs indicadores**:
```
D12 (PB6)  →  R220Ω  →  LED Verde  →  GND   (alarma armada)
D13 (PB7)  →  R220Ω  →  LED Rojo   →  GND   (alarma disparada)
```

> El código usa **flanco de subida** para detectar los switches: cuando el switch cierra, el pin sube de LOW (por la R10kΩ a GND) a HIGH (conectado a 5V). En Proteus, usar un botón `BUTTON` o un DIP switch. Al cerrarse → detona la ISR correspondiente.

### 5.6 Conexión Motor PAP Garaje (Puerto G + ULN2003)

```
Arduino Mega  →  ULN2003A  →  Motor 28BYJ-48
──────────────────────────────────────────────
D41 (PG0)    →  IN1       →  Bobina A
D40 (PG1)    →  IN2       →  Bobina B
D39 (PG2)    →  IN3       →  Bobina C
D38 (PG3)    →  IN4       →  Bobina D
GND          →  GND (ULN2003 lado lógico)
             →  VCC motor del driver (5V o 12V según motor)
```

> El ULN2003A es un driver Darlington que amplifica la corriente. El Mega solo entrega ~40mA por pin; el motor PAP puede necesitar 200-500mA. En Proteus, el componente de motor generalmente ya incluye el driver internamente.

**Número de pasos**: `MOTOR_PASOS_GARAJE = 512` (ajustar en `motor.h` si el motor del entrenador necesita más o menos pasos para el recorrido completo).

### 5.7 Conexión Temperatura (Puerto K + ADC)

```
Potenciómetro 10kΩ:
  Pin 1  →  5V
  Pin 2  →  A9 (PK1) ← ⚠ CONECTAR A A9, no A8
  Pin 3  →  GND

Calefactor (LED rojo):
  A11 (PK3)  →  R220Ω  →  LED Rojo  →  GND

Ventilador (LED azul):
  A12 (PK4)  →  R220Ω  →  LED Azul  →  GND
```

**Relación ADC ↔ Temperatura**: el código usa la fórmula `temp = (ADC * 40) / 1023`.
- Potenciómetro al mínimo (0V) → ADC = 0 → 0°C
- Potenciómetro al máximo (5V) → ADC = 1023 → 40°C

**Umbrales de control** (configurables en `temperatura.h`):
- Calefactor ON cuando temp < 18°C · OFF cuando temp > 20°C
- Ventilador ON cuando temp > 26°C · OFF cuando temp < 24°C

### 5.8 Conexión Dimmer (Puerto B — OC1A)

```
D11 (PB5/OC1A)  →  R220Ω  →  LED Amarillo  →  GND
```

La señal PWM sale del Timer1 directamente al pin D11. No requiere lógica externa.

---

## 6. Compilación y Carga del Firmware

### En Arduino IDE

1. Abrir `ProyectoDomotico.ino` (Arduino IDE detecta los `.cpp` y `.h` del mismo directorio automáticamente).
2. Seleccionar placa: `Tools → Board → Arduino AVR Boards → Arduino Mega or Mega 2560`.
3. Seleccionar procesador: `Tools → Processor → ATmega2560`.
4. **Para Proteus**: `Sketch → Export Compiled Binary` → usar el archivo `.hex` generado en la carpeta `build/arduino.avr.mega/`.
5. **Para hardware físico**: seleccionar puerto COM y `Sketch → Upload`.

### Cargar en Proteus

1. Hacer clic derecho sobre el componente ATmega2560 → `Edit Properties`.
2. En `Program File` seleccionar el `.hex` de la carpeta `build/arduino.avr.mega/`.
3. Verificar que `Clock Frequency` esté en **16MHz** (frecuencia del cristal del Mega).
4. Iniciar la simulación con el botón `Play` (▶).

---

## 7. Plan de Pruebas

### Prueba 1 — LCD

**Objetivo**: verificar que el LCD muestra el texto de bienvenida y acepta actualizaciones.

**Resultado esperado al iniciar la simulación**:
- Línea 0: `ALARMA: DESACTIV`
- Línea 1: `Codigo: ____`

**Cómo verificar**:
1. Iniciar la simulación.
2. La pantalla debe mostrar el mensaje inicial dentro de los primeros 2 segundos.
3. Después de `INTERVALO_TEMP` ciclos (~2 segundos), la línea 0 cambia a `Temp: XXC` con la temperatura actual.

**Si no aparece nada**: revisar conexiones de PA0, PA2, PA4-PA7. Verificar contraste del LCD (pin V0 conectado a potenciómetro o a GND).

---

### Prueba 2 — Teclado

**Objetivo**: verificar que las pulsaciones se detectan correctamente.

**Pasos**:
1. Con la simulación corriendo, presionar la tecla `1` en el teclado.
2. La línea 1 del LCD debe mostrar `Codigo: *___` (un asterisco).
3. Presionar `2`, `3`, `4`.
4. La línea 1 debe mostrar `Codigo: ****`.

**Prueba de teclas de función**:
- Presionar `C` → la línea 1 muestra `Luz: 1/10` y la terminal serial muestra `LUZ:1`.
- Presionar `C` varias veces → el nivel sube hasta `Luz: 10/10`.
- Presionar `D` → el nivel baja de 1 en 1.

**Si el teclado no responde**: verificar pines PL0-PL3 (columnas) y PC0-PC3 (filas). El Timer2 debe estar corriendo (no se puede verificar visualmente; confirmar que el código compiló sin errores).

---

### Prueba 3 — USART (Terminal Virtual)

**Objetivo**: verificar envío y recepción de comandos por serial.

**Pasos**:
1. Abrir la Virtual Terminal en Proteus (doble clic sobre ella en la simulación).
2. Al iniciar la simulación, debe aparecer:
   ```
   Comandos: ARM / DISARM:xxxx / LUZ:0-10 / GARAJE:ABRIR / GARAJE:CERRAR
   ```
3. Escribir `ARM` + Enter en la terminal → respuesta: `OK:ARMADA`, LCD línea 0: `ALARMA: ARMADA`.
4. Escribir `DISARM:1234` + Enter → respuesta: `OK:DESACTIVADA`.
5. Escribir `LUZ:5` + Enter → respuesta: `OK:LUZ,5`, LCD línea 1: `Luz: 5/10`.
6. Escribir `disarm:1234` (minúsculas) + Enter → mismo resultado que en mayúsculas (la función `a_mayusculas` lo convierte).

**Si no llegan datos a la terminal**: confirmar que TX del Mega (D1) va a RXD de la terminal (cruzado) y que la velocidad es 9600 bps.

---

### Prueba 4 — Alarma de Acceso

**Objetivo**: verificar el ciclo arm → sensor → disparo → desarm.

**Pasos**:
1. Enviar `ARM` por la terminal serial.
2. LCD línea 0 muestra `ALARMA: ARMADA`. LED verde (D12/PB6) enciende.
3. Activar el DIP-SW1 (SW1 → D19/INT2) — simula apertura de puerta.
4. LCD línea 0 muestra `!! INTRUSION !!`. LED rojo (D13/PB7) enciende.
5. Terminal muestra `ALARMA:ACCESO`.
6. Enviar `DISARM:1234` → LED rojo apaga, LCD muestra `ALARMA: DESACTIV`.

**Prueba con código incorrecto**:
1. Presionar en el teclado `1`, `2`, `3`, `5` (código incorrecto).
2. Presionar `A` (armar).
3. LCD muestra `CODIGO INCORRECTO`. Terminal muestra `ERROR:CODIGO`.

---

### Prueba 5 — Alarma de Incendio

**Objetivo**: verificar que la alarma de incendio se dispara independientemente del estado de la alarma de acceso.

**Pasos**:
1. NO enviar `ARM` (alarma de acceso desactivada).
2. Activar el DIP-SW2 posición 1 (SW3 → D2/INT4) — simula sensor de humo.
3. LCD línea 0 muestra `!! INCENDIO !!`. LED rojo enciende.
4. Terminal muestra `ALARMA:INCENDIO`.
5. Para silenciar: enviar `DISARM:1234` (apaga el LED rojo aunque el acceso no estaba armado).

**Nota**: la alarma de incendio se dispara aunque la de acceso esté desactivada, cumpliendo con el requisito del proyecto (prioridad máxima).

---

### Prueba 6 — Motor PAP Garaje

**Objetivo**: verificar apertura y cierre del garaje por comando serial.

**Pasos**:
1. Enviar `GARAJE:ABRIR` + Enter.
2. Terminal muestra `OK:GARAJE_ABRIENDO`. El motor comienza a girar.
3. Después de 512 pasos, terminal muestra `OK:GARAJE_ABIERTO`.
4. Enviar `GARAJE:CERRAR` → proceso inverso.

> **Nota**: La función `motor_pasos()` es **bloqueante** (~2.5 segundos para 512 pasos a 5ms/paso). Durante ese tiempo el teclado no responde, pero las interrupciones de alarma **sí siguen funcionando** (las ISR de INT2-INT5 interrumpen incluso el delay de `_delay_ms()`).

**En Proteus**: el motor debe girar visiblemente en el sentido correcto. Si no se mueve, verificar pines PG0-PG3 y la conexión con el driver.

---

### Prueba 7 — Control de Temperatura

**Objetivo**: verificar lectura ADC y control de calefactor y ventilador.

**Pasos**:
1. El LCD actualiza la temperatura cada ~2000 ciclos del loop.
2. Girar el potenciómetro de A9 hacia el mínimo (0V) → LCD muestra `Temp: 0C` → si el sistema está frío, el LED del calefactor (A11/PK3) enciende.
3. Girar el potenciómetro a 45% de recorrido → temperatura ~18°C → zona neutra (ninguno activo si se estaba en calefactor ON: apaga cuando pasa 20°C).
4. Girar a 65% de recorrido → temperatura ~26°C → LED del ventilador (A12/PK4) enciende.
5. Girar a 60% → temperatura ~24°C → ventilador apaga.

**Tabla de zonas de temperatura**:

```
0°C ───── 18°C ──── 20°C ─────── 24°C ──── 26°C ──── 40°C
           ↑          ↑            ↑           ↑
       Calef.ON   Calef.OFF    Vent.OFF    Vent.ON
       (si apag)  (si encend)  (si encend) (si apag)
```

**Si la temperatura siempre muestra 0 o 40**: verificar que el potenciómetro esté conectado a **A9** (no A8). Revisar la conexión al pin correcto en Proteus.

---

### Prueba 8 — Dimmer de Iluminación

**Objetivo**: verificar control del brillo por teclado y por serial.

**Por teclado**:
1. Presionar `C` → LCD muestra `Luz: 1/10`. Terminal muestra `LUZ:1`. El LED en D11 enciende con bajo brillo.
2. Presionar `C` varias veces hasta `Luz: 10/10` → mayor brillo.
3. Presionar `D` → el nivel baja.

**Por serial**:
1. Enviar `LUZ:7` → LCD muestra `Luz: 7/10`. Terminal responde `OK:LUZ,7`.
2. Enviar `LUZ:0` → LED apaga completamente.

> **Nota Proteus**: el cambio de brillo visible en la simulación es sutil. El dimmer está calibrado para la zona 0%-7.8% de duty cycle donde Proteus sí distingue cambios. En el hardware físico, el rango completo es perfectamente visible.

---

### Prueba 9 — Integración

**Objetivo**: verificar que todos los módulos funcionan simultáneamente sin interferencias.

**Secuencia de prueba completa**:
1. Iniciar simulación → verificar LCD con estado inicial y temperatura.
2. Presionar `C` tres veces → nivel de luz = 3.
3. Ingresar código `1`, `2`, `3`, `4` en teclado → presionar `A` → alarma ARMADA.
4. Activar SW3 (humo) → verificar que la alarma de incendio se dispara.
5. Enviar `DISARM:1234` para silenciar.
6. Enviar `LUZ:5` → verificar LCD y LED.
7. Enviar `GARAJE:ABRIR` → mientras el motor gira, presionar una tecla del teclado.
8. Verificar que la tecla fue registrada (al terminar el movimiento del motor, el LCD la muestra o la terminal la procesa).

**Resultado esperado**: todos los módulos coexisten sin errores. El motor bloquea el loop pero no bloquea las interrupciones.

---

## 8. Prueba en Hardware Físico (Entrenador)

### Diferencias respecto a Proteus

| Aspecto | Proteus | Hardware físico |
|---------|---------|-----------------|
| Dimmer | Solo visible en rango 0-7.8% duty | Rango completo 0-100% visible |
| Motor | Puede no girar si el modelo no es exacto | Verificar `MOTOR_PASOS_GARAJE` (512 puede no ser suficiente) |
| ADC temperatura | Pot en A9 (⚠ no A8) | Mismo pin, misma conexión |
| Velocidad motor | Simulación puede ser más rápida/lenta | Ajustar `_delay_ms(5)` en `motor_pasos()` si vibra o no gira |

### Pasos para hardware físico

1. **Compilar y grabar el .hex** con Arduino IDE (`Sketch → Upload`) con el Mega conectado por USB.
2. **Conectar los periféricos** según el mapa de pines del manual.
3. **Abrir el monitor serial** de Arduino IDE o PuTTY a 9600 bps.
4. Verificar el mensaje de bienvenida en la terminal y en el LCD.
5. Ejecutar las pruebas del mismo orden que en Proteus (secciones 7.1 a 7.9).

### Ajustes específicos para el entrenador

- **Motor PAP**: si el motor no gira o vibra, probar `_delay_ms(2)` (más rápido) o `_delay_ms(10)` (más torque) en `motor.cpp:82`.
- **LCD contraste**: ajustar el potenciómetro de contraste conectado al pin V0 del LCD.
- **Sensor de temperatura**: el entrenador puede tener un LM35 o NTC real. Adaptar la fórmula de conversión en `temp_celsius()` según el sensor específico.

---

## 9. Errores Comunes y Soluciones

| Síntoma | Causa probable | Solución |
|---------|---------------|----------|
| LCD no muestra nada | Pines PA0-PA7 incorrectos o contraste V0 | Verificar conexiones y ajustar V0 |
| Teclado no responde | Timer2 no inicializado o pines PL/PC invertidos | Verificar `teclado_init()` fue llamado antes de `sei()` |
| Terminal serial vacía | TX/RX no cruzados o baud rate distinto | Cruzar cables y confirmar 9600 bps |
| Alarma de acceso nunca dispara | La alarma no fue ARMADA primero | Enviar `ARM` antes de activar el switch |
| Alarma de incendio nunca dispara | INT4/INT5 no habilitadas | Revisar que `alarma_init()` es llamado |
| Motor no gira | Secuencia de pines PG0-PG3 incorrecta | Verificar orden de bobinas con el motor específico |
| Temperatura siempre 0°C | Pot conectado a A8 en lugar de A9 | Mover conexión a A9 (PK1) |
| Temperatura siempre 40°C | Pot conectado al extremo incorrecto | Intercambiar extremos GND/5V del potenciómetro |
| Dimmer no cambia brillo visible | Rango dentro de Proteus es limitado | Normal; en hardware físico es completamente visible |
| `CODIGO INCORRECTO` con código correcto | Mayúsculas/minúsculas en serial | Enviar el código en minúsculas también funciona (el código de alarma es "1234") |
| Compilación falla | Archivos stub incluidos (RFID, EEPROM) | Verificar que los `#include` de fase 4 y 7 estén comentados en el `.ino` |

---

## 10. Código de Alarma por Defecto

El código está definido en `alarma.cpp` línea 20:
```c
static const char codigo_alarma[ALARMA_CODIGO_LEN + 1] = "1234";
```
Para cambiar el código, modificar esta cadena y recompilar.

---

*Sistema Domótico — Proyecto Microcontroladores 2026-1*
*ATmega2560 (Arduino Mega 2560) · Registros directos sin librerías externas*
