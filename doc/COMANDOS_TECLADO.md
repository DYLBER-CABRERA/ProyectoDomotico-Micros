# Comandos del sistema — teclado y serial (USART)

Sistema Domótico sobre **ATmega2560**. Se controla de dos formas equivalentes:

- **Teclado 4×4** (entrenador físico): un *modo comando* con secuencias de teclas.
- **Terminal serial / USART** a **9600 8N1** (Proteus o monitor serie): comandos de texto.

Mismo firmware para ambos escenarios. Esta es la referencia completa y actualizada.

---

## 1. Modo comando por teclado

```
   *  +  [código de función de 2 dígitos]  +  [parámetros]  +  #
```

| Tecla | Significado |
|-------|-------------|
| `*` | **Inicia** el comando. Pulsada de nuevo, **cancela**. |
| `0`–`9` | Código de función y parámetros. |
| `A` | **Separador** de parámetros (horno, mercado-agregar, enrolar hijo, recargar). |
| `#` | **Ejecuta** el comando. |

Mientras escribes, el LCD muestra `CMD:` con lo que llevas tecleado (en la línea 0). Si te
equivocas, pulsa `*` para cancelar.

**Ejemplo:** encender el sonido al nivel 7 → `*` `2` `1` `7` `#` (función `21`, parámetro `7`).

---

## 2. Teclas directas (sin `*`)

| Tecla | Acción |
|-------|--------|
| `0`–`9` | Teclear el **código de la alarma** (4 dígitos). |
| `A` | **Armar** la alarma de acceso (tras teclear el código correcto). |
| `B` | **Desarmar** la alarma de acceso (tras teclear el código correcto). |
| `C` | **Subir** la luz un nivel (dimmer +1, máx. 10). |
| `D` | **Bajar** la luz un nivel (dimmer −1, mín. 0). |

> Armar por teclado directo: `1` `2` `3` `4` y luego `A`. (Código por defecto: **1234**.)

---

## 3. Tabla de comandos por teclado (`*` … `#`)

| Función | Secuencia | Ejemplo | Notas |
|---------|-----------|---------|-------|
| **Garaje abrir** | `*11#` | `*11#` | — |
| **Garaje cerrar** | `*10#` | `*10#` | — |
| **Sonido encender** | `*21‹vol›#` | `*217#` → vol 7 | vol 0–10 |
| **Sonido apagar** | `*20#` | `*20#` | — |
| **Volumen +/−** | `*22#` / `*23#` | `*22#` | solo con el sonido encendido |
| **Horno encender** | `*31‹temp›A‹seg›#` | `*31180A5#` → 180 °C, **5 s** | temp y **segundos** (0–255) |
| **Horno apagar** | `*30#` | `*30#` | — |
| **Luz a nivel fijo** | `*41‹nivel›#` | `*417#` → nivel 7 | nivel 0–10 |
| **Mercado: agregar** | `*51‹cod›A‹cant›#` | `*5101A2#` → leche ×2 | si ya existe, **reemplaza** la cantidad |
| **Mercado: eliminar** | `*50‹cod›#` | `*5001#` → quita leche | cod = producto (§6) |
| **Mercado: listar** | `*59#` | `*59#` | lista por serial; conteo en LCD |
| **Enrolar adulto** | `*61‹código›#` | `*611234#` | luego pasar la tarjeta |
| **Enrolar hijo** (tiempo) | `*62‹segundos›A‹código›#` | `*62300A1234#` → 300 s | el tiempo se guarda en **EEPROM**, ya no en la tarjeta |
| **Recargar tiempo hijo** | `*63‹segundos›A‹código›#` | `*6360A1234#` → +60 s | suma segundos al tiempo restante del hijo |
| **Borrar tarjeta** | `*60‹código›#` | `*601234#` | luego pasar la tarjeta a borrar |
| **Alarma armar** | `*91‹código›#` | `*911234#` | equivale a la tecla `A` |
| **Alarma desarmar** | `*90‹código›#` | `*901234#` | equivale a la tecla `B` |

> Las funciones RFID (`6x`) y de alarma (`9x`) **exigen el código** de la alarma.

### Memoria rápida de los códigos de función

```
1x = GARAJE    11 abrir       · 10 cerrar
2x = SONIDO    21 ON+vol      · 20 OFF · 22 vol+ · 23 vol-
3x = HORNO     31 ON+temp+SEG · 30 OFF
4x = LUZ       41 nivel directo (0-10)
5x = MERCADO   51 agregar     · 50 quitar · 59 listar
6x = RFID      61 enrol adulto · 62 enrol hijo · 63 recargar · 60 borrar
9x = ALARMA    91 armar       · 90 desarmar
```

---

## 4. Comandos por serial (USART, 9600 8N1)

Se escriben como texto y se terminan con Enter. No distinguen mayúsculas/minúsculas.

| Comando | Acción | Respuesta OK | Error |
|---------|--------|--------------|-------|
| `ARM:1234` | Armar alarma (requiere código) | `OK:ARMADA` | `ERROR:CODIGO` |
| `DISARM:1234` | Desarmar alarma | `OK:DESACTIVADA` | `ERROR:CODIGO` |
| `LUZ:0..10` | Fijar nivel del dimmer | `OK:LUZ,N` | — |
| `GARAJE:ABRIR` / `GARAJE:CERRAR` | Mover el garaje | `OK:GARAJE_ABIERTO` / `..._CERRADO` | — |
| `HORNO:‹temp›,‹seg›` | Encender horno (ej. `HORNO:180,5` = 5 s) | `OK:HORNO,temp,seg` | — |
| `SONIDO:ON[,vol]` / `SONIDO:OFF` | Equipo de sonido | `OK:SONIDO_ON,vol` / `OK:SONIDO_OFF` | — |
| `VOL:N` / `VOL:+` / `VOL:-` | Volumen (con sonido ON) | `OK:VOL,N` | `ERROR:SONIDO_APAGADO` |
| `MERCADO:ADD,nombre,cant` | Agregar / **actualizar** producto | `OK:MERCADO_AGREGADO` o `OK:MERCADO_ACTUALIZADO` | `ERROR:MERCADO_LLENO` / `ERROR:FORMATO_MERCADO` |
| `MERCADO:DEL,nombre` | Eliminar producto | `OK:MERCADO_ELIMINADO` | `ERROR:MERCADO_NO_ENCONTRADO` |
| `MERCADO:LIST` | Listar productos | (lista) | `LISTA VACIA` |
| `ENROL:ADULTO,‹cod›` | Enrolar próxima tarjeta como adulto | `OK:ENROLANDO_ADULTO` → `OK:ENROLADO_ADULTO` | `ERROR:CODIGO` / `ERROR:EEPROM_LLENA_O_DUPLICADO` |
| `ENROL:HIJO,‹seg›,‹cod›` | Enrolar hijo con N segundos de tiempo (en EEPROM) | `OK:ENROLANDO_HIJO,N` → `OK:ENROLADO_HIJO,N` | idem |
| `BORRAR,‹cod›` | Borrar la próxima tarjeta presentada | `OK:BORRANDO` → `OK:BORRADO` | `ERROR:CODIGO` / `ERROR:UID_NO_EXISTE` |
| `ACCESOS:‹seg›,‹cod›` | Sumar N segundos al tiempo restante del hijo (en EEPROM) | `OK:RECARGANDO,N` → `OK:ACCESOS,N` | `ERROR:CODIGO` / `ERROR:UID_NO_ES_HIJO` |

> **Importante:** enrolar, borrar y recargar funcionan en **dos pasos** — primero el
> comando (teclado o serial), luego se **acerca la tarjeta** al lector RC522.

Alertas que el sistema envía solo: `ALARMA:INCENDIO`, `ALARMA:ACCESO`, `ALARMA:INTRUSO`,
`HORNO:FIN`, `ACCESO:CONCEDIDO_PUERTA` / `..._GARAJE` / `..._ADULTO` / `..._HIJO,N`,
`ACCESO:DENEGADO`, `ACCESO:DENEGADO_SIN_CUPOS`.
Alertas del tiempo de sala: `HIJO:TIME_UP,<indice>`, `ACCESO:HIJO_ENTRA,<indice>,<t>`,
`ACCESO:HIJO_SALE,<indice>,<t>` (t negativo = sobregiro).

---

## 5. Acceso por RFID y los 3 lugares (2 pulsadores)

Hay **3 puntos de acceso**: **puerta principal**, **garaje** y **sala de juegos**. Para
simular frente a cuál estás, se usan **2 pulsadores**:

- **Pulsador izquierdo → D40**: retrocede en la lista.
- **Pulsador derecho → D39**: avanza en la lista.
- Lista circular: **PUERTA → GARAJE → SALA → PUERTA → …**
- Al pulsar, el LCD muestra `LUGAR: PUERTA / GARAJE / SALA`.

Al acercar una **tarjeta válida**, la acción depende del lugar:

| Lugar | Acción al pasar la tarjeta |
|-------|----------------------------|
| **Puerta principal** | Concede acceso (abre el imán). Si la alarma está **armada**, arranca un **temporizador de 10 s** para desarmar con el código. |
| **Garaje** | Concede acceso (mueve el servo). Si está **armada**, **15 s** para desarmar. |
| **Sala de juegos** | **Adulto**: entra libre. **Niño**: toggle entrada/salida. Al entrar, inicia el descuento de tiempo (cada segundo resta 1). Al salir, se detiene. El tiempo se guarda en **EEPROM** (ya no en la tarjeta). Si se acaba → `HIJO:TIME_UP` y sigue contando **negativo** (sobregiro). |

> **Retardo de desarme**: tras conceder acceso en puerta/garaje con la alarma armada, el
> LCD muestra la cuenta regresiva `DESARMA EN: Ns`. Si **desarmas a tiempo** con el código,
> todo bien. Si **se vence el tiempo**, se dispara la **alarma de intrusión** (`!! INTRUSO !!`,
> LED rojo, `ALARMA:INTRUSO`).

---

## 6. Catálogo de productos del mercado (por teclado)

Como en el teclado no se escriben nombres, cada producto tiene un **código de 2 dígitos**:

| Código | Producto | Código | Producto |
|:------:|----------|:------:|----------|
| `01` | leche | `06` | azucar |
| `02` | pan | `07` | cafe |
| `03` | huevos | `08` | sal |
| `04` | arroz | `09` | pasta |
| `05` | aceite | `10` | agua |

Para ampliar: editar `producto_por_codigo()` en `ProyectoDomotico.ino` y añadir la fila aquí.

**Ejemplos:** agregar 3 panes → `*5102A3#` · quitar el café → `*5007#` · listar → `*59#`.
Si un producto ya está en la lista y lo agregas con otra cantidad, **se reemplaza** la
cantidad (no se duplica) y el LCD muestra `‹producto› ACTUALIZADO`.

---

## 7. Realimentación en el LCD

- **Línea 0**: estado de la alarma (`ALARMA: DESACTIV` / `ARMADA` / `!! INTRUSO !!` …) y,
  de forma puntual, los mensajes de comandos (CMD, acceso RFID, lugar, garaje, etc.).
- **Línea 1**: temperatura (`T:NNC`, izquierda) y nivel de luz (`L:N/10`, derecha).
- Si el ruido del entrenador llega a corromper la pantalla, el firmware la **re-sincroniza
  y repinta sola** a los pocos segundos cuando está en reposo.

**Anti-intrusos:** teclear mal el código **3 veces seguidas** dispara la alarma de intruso
(`!! INTRUSO !!` + LED rojo + `ALARMA:INTRUSO`). Un código correcto reinicia el contador.

**Hijo de pruebas (Fase 0):** el pulsador en **D7 (PH4)** simula la presentación del
UID `{0x00,0x00,0x00,0x01}`. Se puede enrolar vía `ENROL:HIJO,<seg>,<cod>` (serial) o
`*62<seg>A<cod>#` (teclado), y luego usar el botón para probar entrada/salida de la sala
sin tener el hardware RFID conectado. El botón también sirve para "presentar la tarjeta"
durante enrolamiento, borrado o recarga.

---

## 8. Equivalencia teclado ↔ serial

| Función | Teclado | Serial |
|---------|---------|--------|
| Armar / desarmar | `A` / `B` (tras código) o `*91‹c›#` / `*90‹c›#` | `ARM:1234` / `DISARM:1234` |
| Luz +/− · nivel fijo | `C` / `D` · `*41‹n›#` | `LUZ:‹n›` |
| Garaje abrir / cerrar | `*11#` / `*10#` | `GARAJE:ABRIR` / `GARAJE:CERRAR` |
| Sonido ON / OFF · vol | `*21‹v›#` / `*20#` · `*22#`/`*23#` | `SONIDO:ON[,v]` / `SONIDO:OFF` · `VOL:…` |
| Horno ON (temp, **seg**) / OFF | `*31‹t›A‹s›#` / `*30#` | `HORNO:‹t›,‹s›` |
| Mercado +/− / lista | `*51‹c›A‹q›#` / `*50‹c›#` / `*59#` | `MERCADO:ADD,…` / `DEL,…` / `LIST` |
| Enrol adulto / hijo (tiempo) | `*61‹c›#` / `*62‹s›A‹c›#` | `ENROL:ADULTO,‹c›` / `ENROL:HIJO,‹s›,‹c›` |
| Recargar tiempo hijo | `*63‹s›A‹c›#` | `ACCESOS:‹s›,‹c›` |
| Borrar tarjeta | `*60‹c›#` | `BORRAR,‹c›` |
| Cambiar de lugar | pulsadores D40 / D39 | — (solo físico) |
| Simular tarjeta hijo | pulsador D7 (activo bajo) | — (solo físico, Fase 0) |

---

*Sistema Domótico — Proyecto Microcontroladores 2026-1 · ATmega2560*
