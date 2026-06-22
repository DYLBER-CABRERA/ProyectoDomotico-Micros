# Feature — Lista de mercado (EEPROM)

- **Fase**: 7c · **Estado**: ✅ · **Archivos**: `mercado.cpp`, `mercado.h`

## Objetivo
Mantener una lista de productos (nombre + cantidad) persistente en EEPROM, consultable y
editable por serial (RF-14).

## Memoria
Base EEPROM `0x0B0`. Hasta `MERCADO_MAX_ITEMS = 10` registros de `12 (nombre) + 1 (\0) +
1 (cantidad) = 14` bytes. Slot vacío = primer byte `0x00`. **No** se solapa con UIDs
(`0x000+`) ni código de alarma (`0x0A0+`).

## API
`mercado_init()`, `mercado_agregar(nombre,cantidad)` (1 ok / 0 lleno o duplicado),
`mercado_eliminar(nombre)`, `mercado_listar()` (envía por serial), `mercado_contar()`.
Comandos `MERCADO:ADD,nom,cant` / `MERCADO:DEL,nom` / `MERCADO:LIST`.

## Comportamiento
`mercado_agregar` rechaza duplicados y busca el primer slot libre; trunca el nombre a 11
chars. Leer-antes-de-escribir protege ciclos de EEPROM. `mercado_listar` imprime
`nombre x cantidad` por línea o `LISTA VACIA`. Usa `extern` de las funciones de `usart`
para evitar dependencia circular.

## Criterios de aceptación
`MERCADO:ADD,leche,2` → `OK:MERCADO_AGREGADO`; `MERCADO:LIST` muestra los productos;
`MERCADO:DEL,leche` → `OK:MERCADO_ELIMINADO`; persiste tras reset.
