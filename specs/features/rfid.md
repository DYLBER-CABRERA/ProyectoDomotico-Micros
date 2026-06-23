# RFID RC522 (Fase 4)

## Dependencias
- `spi_master` (SPI por hardware, PB0–PB3, PB0 como CS)
- `eeprom_mgr` (consulta de UIDs autorizados)
- `acceso` (concesión/denegación de acceso)

## API

```c
void rfid_init();
uint8_t rfid_hay_tarjeta();
uint8_t rfid_leer_uid(uint8_t* uid[4]);
uint8_t rfid_leer_contador(uint8_t* uid, uint8_t* valor);
uint8_t rfid_escribir_contador(uint8_t* uid, uint8_t valor);
void rfid_verificar();

void rfid_entrar_modo_enrol(uint8_t tipo, uint8_t cupos);
void rfid_entrar_modo_borrar();
void rfid_entrar_modo_recarga(uint8_t cantidad);
```

## Comportamiento

- `rfid_init()`: soft-reset RC522, configura timer de timeout, enciende antena.
- `rfid_hay_tarjeta()`: envía REQA (0x26) de 7 bits. Retorna 1 si hay respuesta.
- `rfid_leer_uid()`: anticolisión cascade level 1 (0x93 + NVB 0x20). Verifica BCC.
  Retorna 4 bytes del UID.
- `rfid_leer_contador()`: autentica con key A (0xFF×6) para el bloque 4, lee 16 bytes,
  extrae el contador del primer byte.
- `rfid_escribir_contador()`: autentica, lee bloque completo, modifica byte 0, escribe
  18 bytes (0xA0 + addr + 16 datos) con preservación del resto del bloque.
- `rfid_verificar()`: máquina de estados no bloqueante llamada desde `loop()`:
  1. **OCIOSO**: busca tarjeta con REQA.
  2. **TARJETA_OK**: lee UID. Si hay operación pendiente (enrol/borrar/recarga), la
     ejecuta y envía `OK:...`/`ERROR:...` por serial. Si no, busca el UID en EEPROM:
     - **Adulto**: `ACCESO:CONCEDIDO_ADULTO`, activa imán.
     - **Hijo con cupos**: descuenta 1, escribe en tarjeta, `ACCESO:CONCEDIDO_HIJO,N`.
     - **Hijo sin cupos**: `ACCESO:DENEGADO_SIN_CUPOS`.
     - **Desconocido**: `ACCESO:DENEGADO`.
  3. **PROCESANDO**: espera a que retiren la tarjeta, luego cierra puerta y vuelve a
     OCIOSO.

## Modos de operación pendiente

| Función | Acción | Respuesta serial |
|---------|--------|-----------------|
| `rfid_entrar_modo_enrol(0, 0)` | Espera tarjeta y la guarda como adulto | `OK:ENROLADO_ADULTO` |
| `rfid_entrar_modo_enrol(1, N)` | Espera tarjeta, guarda como hijo, escribe N cupos | `OK:ENROLADO_HIJO,N` |
| `rfid_entrar_modo_borrar()` | Espera tarjeta y borra su UID de EEPROM | `OK:BORRADO` |
| `rfid_entrar_modo_recarga(N)` | Espera tarjeta hijo, suma N al contador | `OK:ACCESOS,N` |

## Protocolo SPI con RC522

- Dirección SPI: `reg << 1` (escritura, bit 0 = 0), `(reg << 1) | 1` (lectura, bit 0 = 1).
- Comunicación: CS bajo → enviar dirección → enviar/recibir datos → CS alto.
- Los comandos de alto nivel (REQA, anticolisión, transceive) se implementan mediante
  el comando `PCD_TRANSCEIVE` (0x0C) del RC522.
