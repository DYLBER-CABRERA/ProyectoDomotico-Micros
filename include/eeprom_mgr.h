#ifndef EEPROM_MGR_H
#define EEPROM_MGR_H
#include <avr/io.h>

// Mapa de memoria EEPROM
// 0x000 - 0x031 : hasta 10 UIDs autorizados (4 bytes c/u + 1 tipo)
// 0x032 - 0x04F : hasta 10 hijos: tiempo restante (int16_t, 2B) + estado (1B)
// 0x0A0 - 0x0A3 : código de alarma (4 dígitos)
// 0x0B0 - 0x1FF : lista de mercado

#define EEPROM_BASE_UIDS      0x000
#define EEPROM_MAX_UIDS       10
#define HIJO_TIEMPO_BASE      0x032     // 10 * 3 = 30 bytes, hasta 0x04F
#define HIJO_TIEMPO_TAM       3         // 2B tiempo (int16_t) + 1B estado
#define EEPROM_BASE_CODIGO    0x0A0
#define EEPROM_BASE_MERCADO   0x0B0

// Estados del hijo respecto a la sala
#define HIJO_FUERA   0
#define HIJO_DENTRO  1

void eeprom_init();
void eeprom_escribir(uint16_t dir, uint8_t dato);
uint8_t eeprom_leer(uint16_t dir);
uint8_t eeprom_buscar_uid(uint8_t* uid);  // retorna índice o 0xFF si no existe
uint8_t eeprom_guardar_uid(uint8_t* uid, uint8_t tipo); // tipo: 0=adulto, 1=hijo
uint8_t eeprom_borrar_uid(uint8_t* uid);

// Tiempo y estado de hijos (sala de juegos)
int16_t  eeprom_leer_tiempo(uint8_t indice);
void     eeprom_escribir_tiempo(uint8_t indice, int16_t tiempo);
uint8_t  eeprom_leer_estado(uint8_t indice);
void     eeprom_escribir_estado(uint8_t indice, uint8_t estado);

#endif
