#ifndef ADC_H
#define ADC_H

// adc.h - Lectura ADC compartida (registros directos ATmega2560)
//
// Varios modulos necesitan el ADC en canales distintos:
//   - temperatura: potenciometro en ADC9 (PK1 / A9)
//   - sonido:      potenciometro de volumen en ADC0 (PF0 / A0)
//
// Este modulo centraliza la configuracion del ADC y permite leer cualquier
// canal conmutando el multiplexor en cada lectura, con una conversion de
// descarte (para que el S/H se estabilice al cambiar de canal) y un
// promediado de 16 muestras (lectura estable, evita parpadeos en los LEDs).

#include <avr/io.h>

// Inicializa el ADC: referencia AVCC (5V), habilitado, prescaler 128
// (16MHz/128 = 125kHz, dentro del rango optimo del ADC). Llamar en setup().
void adc_init();

// Lee el canal indicado (0-15) y devuelve el promedio de 16 muestras (0-1023).
// canal 0-7  -> Puerto F (ADC0-ADC7)
// canal 8-15 -> Puerto K (ADC8-ADC15, requieren MUX5=1)
uint16_t adc_leer(uint8_t canal);

#endif
