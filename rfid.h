#ifndef RFID_H
#define RFID_H
#include <avr/io.h>

#define UID_LEN 4

void rfid_init();
uint8_t rfid_hay_tarjeta();              // 1 si hay tarjeta cerca
uint8_t rfid_leer_uid(uint8_t* uid);     // llena uid[4], retorna 1 si OK
uint8_t rfid_leer_contador(uint8_t* uid, uint8_t* valor);
uint8_t rfid_escribir_contador(uint8_t* uid, uint8_t valor);
void rfid_verificar();                   // llamada desde loop()

#endif
