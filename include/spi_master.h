#ifndef SPI_MASTER_H
#define SPI_MASTER_H
#include <avr/io.h>

// Pines SPI (Puerto B): SS=PB0, SCK=PB1, MOSI=PB2, MISO=PB3
#define SPI_DDR      DDRB
#define SPI_PORT     PORTB
#define SPI_PIN      PINB
#define SPI_SCK      PB1
#define SPI_MOSI     PB2
#define SPI_MISO     PB3
#define SPI_CS       PB0

void spi_init();
uint8_t spi_transferir(uint8_t dato);  // envía y recibe 1 byte
void spi_cs_bajar();   // CS = 0 (seleccionar RC522)
void spi_cs_subir();   // CS = 1 (deseleccionar)

#endif
