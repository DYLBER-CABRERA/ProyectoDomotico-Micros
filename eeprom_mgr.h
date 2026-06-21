#ifndef EEPROM_MGR_H
#define EEPROM_MGR_H
#include <avr/io.h>

// Mapa de memoria EEPROM
// 0x000 - 0x09F : hasta 10 UIDs autorizados (4 bytes c/u + 1 tipo)
// 0x0A0 - 0x0A3 : código de alarma (4 dígitos)
// 0x0B0 - 0x1FF : lista de mercado

#define EEPROM_BASE_UIDS    0x000
#define EEPROM_MAX_UIDS     10
#define EEPROM_BASE_CODIGO  0x0A0
#define EEPROM_BASE_MERCADO 0x0B0

void eeprom_init();
void eeprom_escribir(uint16_t dir, uint8_t dato);
uint8_t eeprom_leer(uint16_t dir);
uint8_t eeprom_buscar_uid(uint8_t* uid);  // retorna índice o 0xFF si no existe
uint8_t eeprom_guardar_uid(uint8_t* uid, uint8_t tipo); // tipo: 0=adulto, 1=hijo
uint8_t eeprom_borrar_uid(uint8_t* uid);

#endif
