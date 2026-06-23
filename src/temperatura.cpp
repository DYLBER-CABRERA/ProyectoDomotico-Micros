// temperatura.cpp - Lectura ADC + control calefactor/ventilador
// Sin librerias - registros directos ATmega2560
// Canal ADC9 (PK1 / pin A9). Conectar el potenciometro a A9 (NO A8).

#include <avr/io.h>
#include "../include/temperatura.h"
#include "../include/adc.h"   // ADC compartido (el potenciometro de temperatura esta en ADC9)


// -- temp_init() -----------------------------------------------------------
// El ADC ahora lo configura adc_init() (en setup) porque es compartido con el
// potenciometro de volumen. Aqui solo se configuran los pines de los actuadores.
void temp_init() {

    // Pines de calefactor y ventilador como salidas DIGITALES (Puerto K).
    // Son salidas on/off puras: el LED enciende o apaga, sin variar el brillo.
    DDRK |= (1 << TEMP_PIN_CALEFACTOR) | (1 << TEMP_PIN_VENTILADOR);
    PORTK &= ~((1 << TEMP_PIN_CALEFACTOR) | (1 << TEMP_PIN_VENTILADOR));
}


// -- temp_leer_adc() ---------------------------------------------------------
// Lee el canal ADC9 (potenciometro de temperatura) mediante el modulo ADC
// compartido, que promedia 16 muestras y descarta la primera tras conmutar
// de canal -> lectura estable, sin parpadeo de los LEDs.
uint16_t temp_leer_adc() {
    return adc_leer(9);
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
    // Encender si: calefactor apagado Y temperatura menor que umbral inferior (18Â°C)
    // Apagar si:   calefactor encendido Y temperatura mayor que umbral superior (20Â°C)
    // La histeresis de 2Â°C (diferencia entre ON=18 y OFF=20) es intencional:
    // evita que el actuador cicle rapido (chattering) cuando la temperatura
    // oscila justo alrededor de un umbral unico, lo que desgastaria el relay/transistor.
    if (!calefactor_encendido && temp_actual < TEMP_CALEFACTOR_ON) {
        PORTK |= (1 << TEMP_PIN_CALEFACTOR);   // |= : pone el bit en 1 â†’ encender calefactor
    } else if (calefactor_encendido && temp_actual > TEMP_CALEFACTOR_OFF) {
        PORTK &= ~(1 << TEMP_PIN_CALEFACTOR);  // &=~ : pone el bit en 0 â†’ apagar calefactor
    }

    // Consultar si el ventilador esta actualmente encendido (mismo mecanismo)
    uint8_t ventilador_encendido = (PORTK & (1 << TEMP_PIN_VENTILADOR)) ? 1 : 0;

    // CONTROL CON HISTERESIS DEL VENTILADOR:
    // Encender si: ventilador apagado Y temperatura mayor que umbral superior (26Â°C)
    // Apagar si:   ventilador encendido Y temperatura menor que umbral inferior (24Â°C)
    // Histeresis de 2Â°C entre OFF=24 y ON=26 por la misma razon que el calefactor.
    if (!ventilador_encendido && temp_actual > TEMP_VENTILADOR_ON) {
        PORTK |= (1 << TEMP_PIN_VENTILADOR);   // encender ventilador
    } else if (ventilador_encendido && temp_actual < TEMP_VENTILADOR_OFF) {
        PORTK &= ~(1 << TEMP_PIN_VENTILADOR);  // apagar ventilador
    }
}

