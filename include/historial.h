// =============================================================================
// historial.h  —  Sustentacion Microprocesadores Historial
// =============================================================================
// Modulo de historial de accesos RFID para el sistema domotico ATmega2560.
//
// PROPOSITO: registrar de forma persistente en la EEPROM interna cada acceso
// concedido por el lector RC522, indicando quien ingreso (adulto o hijo),
// por donde (puerta, garaje o sala) y con que tarjeta (UID de 4 bytes).
// El historial se consulta por serial con el comando HISTORIAL y se borra
// con HISTORIAL:LIMPIAR (o por teclado con *71# listar / *70# limpiar).
//
// ALMACENAMIENTO: ring buffer de 20 entradas en EEPROM interna (0x200-0x279).
// Al llenarse, la entrada mas antigua se sobreescribe (FIFO circular).
// Los datos sobreviven reinicios y cortes de alimentacion.
//
// Quien llama registrar: rfid.cpp, en cada punto de acceso concedido.
// Quien llama listar/limpiar: ProyectoDomotico.ino (comandos serial/teclado).
// =============================================================================

#ifndef HISTORIAL_H
#define HISTORIAL_H
#include <avr/io.h>

// Tipos de acceso que se registran en el historial.
// Coinciden con el byte de tipo en EEPROM de eeprom_mgr (0=adulto, 1=hijo),
// lo que permite comparar directamente el valor leido del slot de UID.
#define HIST_TIPO_ADULTO  0
#define HIST_TIPO_HIJO    1

void historial_init();
void historial_registrar(uint8_t tipo, uint8_t lugar, uint8_t* uid);
void historial_listar();   // vuelca el historial completo por USART
void historial_limpiar();  // borra todas las entradas (head=0, count=0)

#endif
