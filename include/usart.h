#ifndef USART_H
#define USART_H

// usart.h - Driver USART0 ATmega2560
// Sin librerias - registros directos
//
// CONEXION FISICA / VIRTUAL TERMINAL:
//   TX0 (PE1, pin 1 del Mega) -> RXD de la Virtual Terminal
//   RX0 (PE0, pin 0 del Mega) -> TXD de la Virtual Terminal
//   GND -> GND (comun)
//
// En el Arduino Mega TX0/RX0 son los pines 1 y 0 del conector
// digital, los mismos que usa el USB. En Proteus se conectan
// a la Virtual Terminal con los cables cruzados (TX->RX, RX->TX).
//
// CONFIGURACION: 9600 bps, 8 bits, sin paridad, 1 stop bit (8N1)
//
// MODULOS QUE DEPENDEN DE ESTE:
//   - alarma.cpp  (enviar alertas a la PC)
//   - horno.cpp   (recibir comandos HORNO:temp,min)
//   - sonido.cpp  (recibir comandos SONIDO:ON,vol)
//   - mercado.cpp (recibir MERCADO:ADD/LIST)
//   - acceso.cpp  (enviar ACCESO:OK / ACCESO:DENEGADO)

#include <avr/io.h>
#include <avr/interrupt.h>

// -- Configuracion de baud rate ---------------------------------------
// F_CPU = 16MHz, baud = 9600
// Formula: UBRR = F_CPU / (16 * baud) - 1 = 16000000/(16*9600) - 1 = 103
#define USART_BAUD    9600
#define USART_UBRR    103

// -- Tamano del buffer circular de recepcion --------------------------
// 32 bytes es suficiente para los comandos del protocolo serial.
// Debe ser potencia de 2 para que la mascara & funcione eficientemente.
#define USART_RX_BUF_SIZE  32
#define USART_RX_BUF_MASK  (USART_RX_BUF_SIZE - 1)  // = 0x1F = 31

// -- API publica ------------------------------------------------------

// Inicializa USART0 a 9600 bps 8N1 con ISR de recepcion habilitada.
// Llamar en setup() antes de sei().
void usart_init();

// Envia un solo caracter por TX0. Bloqueante: espera que el
// registro de transmision este libre antes de escribir.
void usart_enviar_char(char c);

// Envia una cadena de texto completa caracter a caracter.
// Termina al encontrar el '\0' del string.
void usart_enviar_string(const char* s);

// Envia un numero entero como texto (ej: 123 -> "123").
void usart_enviar_int(int16_t n);

// Envia un salto de linea (\r\n) para compatibilidad con
// terminales seriales en Windows.
void usart_enviar_newline();

// Retorna 1 si hay datos disponibles en el buffer de recepcion,
// 0 si el buffer esta vacio.
uint8_t usart_hay_dato();

// Retorna el siguiente byte del buffer de recepcion y lo consume.
// Si no hay datos retorna '\0'. Verificar usart_hay_dato() primero.
char usart_recibir_char();

// Retorna 1 si el buffer tiene una linea completa (termina en '\n'),
// 0 si no. Util para el parser de comandos.
uint8_t usart_hay_linea();

// Copia la linea completa del buffer a dest[] y la consume.
// dest debe tener al menos USART_RX_BUF_SIZE bytes.
// Retorna la cantidad de caracteres copiados.
uint8_t usart_leer_linea(char* dest);

#endif
