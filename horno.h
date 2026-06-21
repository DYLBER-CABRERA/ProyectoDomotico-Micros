#ifndef HORNO_H
#define HORNO_H
#include <avr/io.h>

void horno_init();
void horno_encender(uint8_t minutos, uint8_t temperatura);
void horno_apagar();
void horno_actualizar();   // llamada desde loop()

#endif
