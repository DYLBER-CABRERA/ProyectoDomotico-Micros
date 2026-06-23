# Gestor de EEPROM para UIDs (Fase 4)

Maneja la zona de EEPROM `0x000–0x09F` para almacenar hasta 10 UIDs autorizados con su
tipo (adulto/hijo). Reutiliza el patrón de `mercado.cpp` (leer-antes-de-escribir para
proteger ciclos de EEPROM).

## Mapa de memoria

| Rango | Contenido | Tamaño |
|-------|-----------|--------|
| `0x000–0x09F` | 10 UIDs (4 bytes c/u + 1 byte tipo) | 5 bytes/slot |
| `0x0A0–0x0A3` | Código de alarma (no administrado aquí) | 4 bytes |
| `0x0B0–0x1FF` | Lista de mercado (`mercado.cpp`) | — |

Formato de cada slot: `[UID0][UID1][UID2][UID3][tipo]`.  
Slot vacío: primer byte = `0xFF`.  
Tipo: `0` = adulto, `1` = hijo.

## API

```c
void eeprom_init();
void eeprom_escribir(uint16_t dir, uint8_t dato);
uint8_t eeprom_leer(uint16_t dir);
uint8_t eeprom_buscar_uid(uint8_t* uid);
uint8_t eeprom_guardar_uid(uint8_t* uid, uint8_t tipo);
uint8_t eeprom_borrar_uid(uint8_t* uid);
```

## Comportamiento

- `eeprom_init()`: no-op (la EEPROM está lista desde el arranque).
- `eeprom_escribir(dir, dato)`: lee antes de escribir; solo escribe si el valor cambia
  (protección de ciclos ~100k).
- `eeprom_leer(dir)`: `eeprom_read_byte()` directo.
- `eeprom_buscar_uid(uid)`: recorre los 10 slots. Retorna índice (0–9) o `0xFF`.
- `eeprom_guardar_uid(uid, tipo)`: verifica duplicado, busca primer slot libre, escribe
  5 bytes. Retorna índice o `0xFF` si llena/duplicado.
- `eeprom_borrar_uid(uid)`: busca, marca primer byte como `0xFF`. Retorna 1 si OK, 0 si
  no existe.
