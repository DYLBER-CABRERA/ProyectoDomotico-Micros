#ifndef MERCADO_H
#define MERCADO_H
#include <avr/io.h>

#define MERCADO_MAX_ITEMS   10
#define MERCADO_NOMBRE_LEN  12

typedef struct {
    char nombre[MERCADO_NOMBRE_LEN];
    uint8_t cantidad;
} ItemMercado;

void mercado_init();
uint8_t mercado_agregar(const char* nombre, uint8_t cantidad);
void mercado_listar();     // envía por USART
void mercado_actualizar(); // llamada desde loop()

#endif
