#ifndef TEMPERATURA_H
#define TEMPERATURA_H

// temperatura.h - Lectura ADC + control calefactor/ventilador
// Sin librerias - registros directos ATmega2560
//
// Un potenciometro en el canal ADC8 simula el sensor de temperatura.
// El valor 0-1023 del ADC se convierte a un rango de temperatura
// simulado de 0 a 40 grados Celsius.
//
// CONEXION FISICA:
//   Potenciometro -> ADC9 (PK1, pin A9)   ← el codigo usa MUX5=1, MUX[4:0]=0x01
//                                            que selecciona ADC9. Conectar el
//                                            potenciometro a A9 en Proteus/hardware.
//                                            (Para ADC8 cambiar 0x01 por 0x00 en temp_init)
//   Calefactor (LED rojo) -> PK3 (pin A11)
//   Ventilador (LED azul) -> PK4 (pin A12)
//
// NOTA: se migro al Puerto K (ADC8-ADC15) para descartar si el problema
// de lectura fija en 1023 era especifico del canal ADC2 (Puerto F)
// o un problema mas general del ADC/componente en Proteus.

#include <avr/io.h>

#define TEMP_PIN_CALEFACTOR  PK3   // pin A11
#define TEMP_PIN_VENTILADOR  PK4   // pin A12

#define TEMP_CALEFACTOR_ON   18
#define TEMP_CALEFACTOR_OFF  20
#define TEMP_VENTILADOR_ON   26
#define TEMP_VENTILADOR_OFF  24

void temp_init();
uint16_t temp_leer_adc();
int8_t temp_celsius();
void temp_controlar();

#endif
