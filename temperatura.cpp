// temperatura.cpp - Lectura ADC + control calefactor/ventilador
// Sin librerias - registros directos ATmega2560
// Version con ADC8 (Puerto K) para comparar contra ADC2 (Puerto F)

#include <avr/io.h>
#include <util/delay.h>
#include "temperatura.h"


// -- temp_init() -----------------------------------------------------------
void temp_init() {

    // ADMUX: ADC Multiplexer Selection Register
    // REFS0=1 -> referencia AVCC (5V)
    // MUX[3:0] = 0000 -> junto con MUX5=1 en ADCSRB, selecciona ADC8
    ADMUX = (1 << REFS0) | 0x01;

    // ADCSRB: MUX5=1 -> necesario para acceder a canales 8-15 (Puerto K)
    ADCSRB = (1 << MUX5);

    // ADCSRA: habilitar ADC, prescaler 128
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

    // Descartar la primera conversion (puede ser no confiable)
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    (void)ADC;

    _delay_ms(2);

    // Pines de calefactor y ventilador como salidas (Puerto K)
    DDRK |= (1 << TEMP_PIN_CALEFACTOR) | (1 << TEMP_PIN_VENTILADOR);
    PORTK &= ~((1 << TEMP_PIN_CALEFACTOR) | (1 << TEMP_PIN_VENTILADOR));
}


// -- temp_leer_adc() ---------------------------------------------------------
// Promedia 8 lecturas para estabilizar el resultado
uint16_t temp_leer_adc() {

    uint32_t suma = 0;
    const uint8_t NUM_LECTURAS = 8;

    for (uint8_t i = 0; i < NUM_LECTURAS; i++) {
        ADCSRA |= (1 << ADSC);
        while (ADCSRA & (1 << ADSC));
        suma += ADC;
    }

    return (uint16_t)(suma / NUM_LECTURAS);
}


// -- temp_celsius() -----------------------------------------------------------
int8_t temp_celsius() {

    uint16_t valor_adc = temp_leer_adc();
    uint32_t temp_calculada = ((uint32_t)valor_adc * 40) / 1023;

    return (int8_t)temp_calculada;
}


void temp_controlar() {

    int8_t temp_actual = temp_celsius(); // leer temperatura actual del sensor ADC

    // Consultar si el calefactor esta actualmente encendido leyendo el bit de PORTK.
    // PORTK & (1 << TEMP_PIN_CALEFACTOR): mascara que aisla el bit del calefactor.
    // El operador ternario ?: convierte el resultado (0 o distinto de 0) a 0 o 1 limpio.
    uint8_t calefactor_encendido = (PORTK & (1 << TEMP_PIN_CALEFACTOR)) ? 1 : 0;

    // CONTROL CON HISTERESIS DEL CALEFACTOR:
    // Encender si: calefactor apagado Y temperatura menor que umbral inferior (18°C)
    // Apagar si:   calefactor encendido Y temperatura mayor que umbral superior (20°C)
    // La histeresis de 2°C (diferencia entre ON=18 y OFF=20) es intencional:
    // evita que el actuador cicle rapido (chattering) cuando la temperatura
    // oscila justo alrededor de un umbral unico, lo que desgastaria el relay/transistor.
    if (!calefactor_encendido && temp_actual < TEMP_CALEFACTOR_ON) {
        PORTK |= (1 << TEMP_PIN_CALEFACTOR);   // |= : pone el bit en 1 → encender calefactor
    } else if (calefactor_encendido && temp_actual > TEMP_CALEFACTOR_OFF) {
        PORTK &= ~(1 << TEMP_PIN_CALEFACTOR);  // &=~ : pone el bit en 0 → apagar calefactor
    }

    // Consultar si el ventilador esta actualmente encendido (mismo mecanismo)
    uint8_t ventilador_encendido = (PORTK & (1 << TEMP_PIN_VENTILADOR)) ? 1 : 0;

    // CONTROL CON HISTERESIS DEL VENTILADOR:
    // Encender si: ventilador apagado Y temperatura mayor que umbral superior (26°C)
    // Apagar si:   ventilador encendido Y temperatura menor que umbral inferior (24°C)
    // Histeresis de 2°C entre OFF=24 y ON=26 por la misma razon que el calefactor.
    if (!ventilador_encendido && temp_actual > TEMP_VENTILADOR_ON) {
        PORTK |= (1 << TEMP_PIN_VENTILADOR);   // encender ventilador
    } else if (ventilador_encendido && temp_actual < TEMP_VENTILADOR_OFF) {
        PORTK &= ~(1 << TEMP_PIN_VENTILADOR);  // apagar ventilador
    }
}
