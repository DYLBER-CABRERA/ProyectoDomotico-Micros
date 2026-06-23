# SPI Maestro (Fase 4)

Bus SPI por hardware del ATmega2560 configurado como maestro para comunicarse con el
módulo RC522 (RFID).

## Recursos de hardware

| Pin Mega | Puerto | Función |
|----------|--------|---------|
| D53 | PB0 | CS/SS (chip select del RC522) |
| D52 | PB1 | SCK (reloj serie, 1 MHz) |
| D51 | PB2 | MOSI (maestro → esclavo) |
| D50 | PB3 | MISO (esclavo → maestro) |

## API

```c
void spi_init();
uint8_t spi_transferir(uint8_t dato);
void spi_cs_bajar();
void spi_cs_subir();
```

## Configuración

- **Modo 0**: CPOL=0, CPHA=0 (muestreo en flanco de subida, cambio en bajada).
- **Velocidad**: `fosc/16` = 1 MHz (prescaler 16). El RC522 admite hasta 10 MHz.
- **Orden**: MSB primero (`DORD=0`).
- **CS**: software manual mediante PB0 (SS configurado como salida para evitar que el
  SPI pase a modo esclavo, requisito del ATmega2560).

## Comportamiento

- `spi_init()`: configura SCK, MOSI y CS como salidas; MISO como entrada. Escribe
  `SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR0)`. CS = 1 (inactivo).
- `spi_transferir(dato)`: carga `SPDR`, espera `SPIF`, retorna `SPDR`.
- `spi_cs_bajar()`: `PORTB &= ~(1<<PB0)`.
- `spi_cs_subir()`: `PORTB |= (1<<PB0)`.
