#ifndef LCD_H
#define LCD_H

// lcd.h - Driver LCD 16x2 HD44780 en modo 4 bits
// Puerto A del ATmega2560
//
// CONEXION FISICA:
//   LCD RS  -> PA0 (pin 22)
//   LCD RW  -> GND (fijo)
//   LCD EN  -> PA2 (pin 24)
//   LCD D4  -> PA4 (pin 26)
//   LCD D5  -> PA5 (pin 27)
//   LCD D6  -> PA6 (pin 28)
//   LCD D7  -> PA7 (pin 29)
//   D0-D3   -> SIN CONECTAR (modo 4 bits)

// avr/io.h - define registros DDRA, PORTA y constantes PA0, PA2...
#include <avr/io.h>

// -- Pines (Puerto A) -------------------------------------------------
#define LCD_DDR    DDRA    // registro de direccion Puerto A
#define LCD_PORT   PORTA   // registro de salida Puerto A
#define LCD_RS     PA0     // Register Select: 0=comando, 1=dato
#define LCD_RW     PA1     // Read/Write: fijo a GND (siempre escribe)
#define LCD_EN     PA2     // Enable: pulso para que el LCD lea los pines
// D4-D7 en PA4-PA7 (los 4 bits altos del puerto)

// -- API publica ------------------------------------------------------
void lcd_init();                          // inicializar el LCD (llamar 1 vez)
void lcd_cmd(uint8_t cmd);                // enviar comando al LCD
void lcd_char(char c);                    // enviar un caracter a mostrar
void lcd_string(const char* s);           // enviar cadena de texto
void lcd_goto(uint8_t fila, uint8_t col); // mover cursor (fila 0 o 1)
void lcd_clear();                         // borrar pantalla
void lcd_int(int16_t n);                  // escribir numero entero

#endif
