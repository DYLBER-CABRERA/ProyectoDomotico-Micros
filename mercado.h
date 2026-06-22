#ifndef MERCADO_H
#define MERCADO_H

// mercado.h - Lista de mercado persistente en EEPROM
// Sin librerias - registros directos ATmega2560 (usa avr/eeprom.h, permitida)
//
// MAPA DE MEMORIA EEPROM (ver tambien eeprom_mgr.h de la Fase 4 -- usa
// una zona DISTINTA para no chocar con los UIDs de tarjetas RFID):
//   Base: 0x0B0 (despues de los 10 UIDs de Fase 4: 10*5=50 bytes,
//                 mas el codigo de alarma de Fase 5: 4 bytes,
//                 deja margen hasta 0x0B0 sin solaparse)
//   Cada producto ocupa MERCADO_TAM_REGISTRO bytes:
//     12 bytes para el nombre (11 caracteres + '\0')
//     1  byte  para la cantidad (0-255)
//   Maximo MERCADO_MAX_ITEMS productos simultaneos
//
// COMANDOS:
//   MERCADO:ADD,<nombre>,<cantidad>  ej. MERCADO:ADD,leche,2
//   MERCADO:DEL,<nombre>             ej. MERCADO:DEL,leche
//   MERCADO:LIST                     envia todos los productos por serial

#include <avr/io.h>
#include <avr/eeprom.h>

#define MERCADO_BASE_EEPROM   0x0B0
#define MERCADO_MAX_ITEMS     10
#define MERCADO_NOMBRE_LEN    11   // 11 caracteres + '\0' = 12 bytes
#define MERCADO_TAM_REGISTRO  (MERCADO_NOMBRE_LEN + 1 + 1) // nombre+'\0'+cantidad

// Marca de slot vacio: un nombre que empieza con '\0' se considera libre
#define MERCADO_SLOT_VACIO    0x00

// -- API publica --------------------------------------------------------

// Inicializa el modulo (no requiere configuracion de hardware)
void mercado_init();

// Agrega un producto a la lista. Busca el primer slot vacio.
// nombre: maximo MERCADO_NOMBRE_LEN caracteres (se trunca si es mas largo)
// cantidad: 0-255
// Retorna 1 si se agrego correctamente, 0 si no hay espacio o ya existe
uint8_t mercado_agregar(const char* nombre, uint8_t cantidad);

// Elimina un producto de la lista por nombre exacto.
// Retorna 1 si se elimino, 0 si no se encontro
uint8_t mercado_eliminar(const char* nombre);

// Envia por USART la lista completa de productos guardados,
// uno por linea, en el formato "nombre x cantidad"
void mercado_listar();

// Retorna la cantidad de productos actualmente guardados (no vacios)
uint8_t mercado_contar();

#endif
