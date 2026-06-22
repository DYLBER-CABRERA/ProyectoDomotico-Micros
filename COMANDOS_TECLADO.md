# Comandos por teclado — Sistema Domótico (entrenador sin terminal)

En el **entrenador físico** solo se dispone del **teclado 4×4** (no hay terminal serial
para escribir comandos como `SONIDO:ON,75`). Por eso el firmware incorpora un **modo
comando por teclado**: una secuencia de teclas activa cada función.

> En **Proteus** sí hay terminal virtual, así que **los comandos serial siguen
> funcionando** además del teclado. Mismo firmware para los dos escenarios.

---

## 1. Cómo funciona el modo comando

```
   *  +  [código de 2 dígitos]  +  [parámetros]  +  #
```

| Tecla | Significado |
|-------|-------------|
| `*` | **Inicia** el comando. Pulsada de nuevo, **cancela**. |
| `0`–`9` | Código de función y parámetros. |
| `A` | **Separador** de parámetros (solo para horno y agregar al mercado). |
| `#` | **Ejecuta** el comando. |

Mientras escribes, el LCD muestra `CMD:` con lo que llevas tecleado. Si te equivocas,
pulsa `*` para cancelar y empieza de nuevo.

**Ejemplo:** encender el equipo de sonido al 75 % →  `*` `2` `1` `7` `5` `#`
(es decir: inicia, función `21` = sonido ON, parámetro `75`, ejecuta).

---

## 2. Teclas directas (sin modo comando)

Estas funcionan en el modo normal, sin `*`, por ser las más frecuentes:

| Tecla | Acción |
|-------|--------|
| `0`–`9` | Teclear el **código de la alarma** (4 dígitos). |
| `A` | **Armar** la alarma de acceso (después de teclear el código correcto). |
| `B` | **Desarmar** la alarma de acceso (después de teclear el código correcto). |
| `C` | **Subir** la luz un nivel (dimmer +1, máx. 10). |
| `D` | **Bajar** la luz un nivel (dimmer −1, mín. 0). |

> Ejemplo armar por teclado directo: `1` `2` `3` `4` luego `A`.

---

## 3. Tabla de comandos (modo `*` … `#`)

| Función | Secuencia | Ejemplo | Parámetros |
|---------|-----------|---------|------------|
| **Garaje abrir** | `*` `11` `#` | `*11#` | — |
| **Garaje cerrar** | `*` `10` `#` | `*10#` | — |
| **Sonido encender** | `*` `21` `#` | `*21#` | el **volumen lo fija el potenciómetro A13** |
| **Sonido apagar** | `*` `20` `#` | `*20#` | — |
| **Horno encender** | `*` `31` `‹temp›` `A` `‹min›` `#` | `*31180A25#` → 180 °C, 25 min | temp y min (0–255) |
| **Horno apagar** | `*` `30` `#` | `*30#` | — |
| **Luz a nivel fijo** | `*` `41` `‹nivel›` `#` | `*415#` → nivel 5 | nivel = 0–10 |
| **Mercado: agregar** | `*` `51` `‹cod›` `A` `‹cant›` `#` | `*5101A2#` → leche ×2 | cod = producto (§4), cant = 0–255 |
| **Mercado: eliminar** | `*` `50` `‹cod›` `#` | `*5001#` → quita leche | cod = producto (§4) |
| **Mercado: listar (LCD)** | `*` `59` `#` | `*59#` | — (muestra la lista en el LCD) |
| **Alarma armar** | `*` `91` `‹código›` `#` | `*911234#` | código de 4 dígitos |
| **Alarma desarmar** | `*` `90` `‹código›` `#` | `*901234#` | código de 4 dígitos |

> El armado/desarmado por `*91`/`*90` es equivalente a las teclas directas `A`/`B`; se
> incluye para tener todo bajo el mismo esquema. Ambos exigen el código (RF-04).

### Memoria rápida de los códigos de función

```
1x = GARAJE      11 abrir   · 10 cerrar
2x = SONIDO      21 ON+vol  · 20 OFF
3x = HORNO       31 ON+temp+min · 30 OFF
4x = LUZ         41 nivel directo (0-10)
5x = MERCADO     51 agregar · 50 quitar · 59 listar
9x = ALARMA      91 armar   · 90 desarmar
```

---

## 4. Catálogo de productos del mercado

Como en el teclado no se pueden escribir nombres, cada producto tiene un **código de 2
dígitos**:

| Código | Producto | Código | Producto |
|:------:|----------|:------:|----------|
| `01` | leche | `06` | azucar |
| `02` | pan | `07` | cafe |
| `03` | huevos | `08` | sal |
| `04` | arroz | `09` | pasta |
| `05` | aceite | `10` | agua |

Para ampliar el catálogo: editar `producto_por_codigo()` en `ProyectoDomotico.ino` y
añadir la fila aquí.

**Ejemplos de mercado:**
- Agregar 3 panes: `*` `51` `02` `A` `3` `#` → `*5102A3#`
- Quitar el café: `*` `50` `07` `#` → `*5007#`
- Ver la lista en el LCD: `*` `59` `#` → `*59#` (muestra cada producto ~1,2 s)

---

## 5. Respuestas y realimentación

- En el **LCD** aparece el resultado de cada comando (p. ej. `SONIDO ON`, `HORNO: ON`,
  `GARAJE ABIERTO`, `MERCADO +`, `CMD INVALIDO`).
- Si hay terminal conectada (Proteus), además se envía la respuesta `OK:…` / `ERROR:…`
  por serial.
- Errores posibles: `CMD INVALIDO` (faltan dígitos del código de función), `CMD
  DESCONOCIDO` (código no existe), `PROD DESCONOCIDO` (código de producto no catalogado),
  `CODIGO INCORRECTO` (código de alarma errado).

> **Anti-intrusos**: si se teclea mal el código de la alarma **3 veces seguidas** (al
> armar o desarmar), el sistema lo interpreta como intento de intrusión y **dispara la
> alarma**: LCD `!! INTRUSO !!`, LED rojo encendido y `ALARMA:INTRUSO` por serial. Un
> código correcto reinicia el contador; para silenciarla, ingresa el código correcto
> (desarmar). El número de intentos se ajusta en `ALARMA_MAX_INTENTOS` (`alarma.h`).

---

## 6. Equivalencia teclado ↔ serial

| Función | Teclado | Serial (Proteus) |
|---------|---------|------------------|
| Armar alarma | `A` (tras código) o `*91‹cod›#` | `ARM:1234` |
| Desarmar alarma | `B` (tras código) o `*90‹cod›#` | `DISARM:1234` |
| Luz +/− | `C` / `D` | `LUZ:0-10` |
| Luz nivel fijo | `*41‹n›#` | `LUZ:‹n›` |
| Garaje abrir/cerrar | `*11#` / `*10#` | `GARAJE:ABRIR` / `GARAJE:CERRAR` |
| Sonido ON/OFF | `*21#` / `*20#` | `SONIDO:ON` / `SONIDO:OFF` (volumen por pot A13) |
| Garaje | `*11#` / `*10#` | `GARAJE:ABRIR` / `GARAJE:CERRAR` (servo, no bloqueante) |
| Horno ON/OFF | `*31‹t›A‹m›#` / `*30#` | `HORNO:‹t›,‹m›` |
| Mercado +/−/lista | `*51‹c›A‹q›#` / `*50‹c›#` / `*59#` | `MERCADO:ADD,…` / `DEL,…` / `LIST` |

---

*Sistema Domótico — Proyecto Microcontroladores 2026-1 · ATmega2560*
