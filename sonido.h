#ifndef SONIDO_H
#define SONIDO_H
#include <avr/io.h>

void sonido_init();
void sonido_set_volumen(uint8_t nivel);  // 0-255 → señal analógica
void sonido_actualizar();

#endif
