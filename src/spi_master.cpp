#include "../include/spi_master.h"

void spi_init() {
    // SCK, MOSI y CS como salidas; MISO como entrada
    SPI_DDR |= (1 << SPI_SCK) | (1 << SPI_MOSI) | (1 << SPI_CS);
    SPI_DDR &= ~(1 << SPI_MISO);

    // CS = 1 (inactivo por defecto)
    SPI_PORT |= (1 << SPI_CS);

    // SPCR: SPE=1 (SPI enable), MSTR=1 (master), CPOL=0, CPHA=0 (modo 0),
    // SPR0=1 (fosc/16 = 1 MHz con prescaler 16, RC522 admite hasta 10 MHz)
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0);
}

uint8_t spi_transferir(uint8_t dato) {
    SPDR = dato;                   // cargar byte a enviar
    while (!(SPSR & (1 << SPIF))); // esperar a que termine la transferencia
    return SPDR;                   // retornar byte recibido
}

void spi_cs_bajar() {
    SPI_PORT &= ~(1 << SPI_CS);
}

void spi_cs_subir() {
    SPI_PORT |= (1 << SPI_CS);
}
