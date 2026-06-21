#ifndef SPI_MASTER_H
#define SPI_MASTER_H
#include <avr/io.h>

void spi_init();
uint8_t spi_transferir(uint8_t dato);  // envía y recibe 1 byte
void spi_cs_bajar();   // SS = 0 (seleccionar RC522)
void spi_cs_subir();   // SS = 1 (deseleccionar)

#endif
