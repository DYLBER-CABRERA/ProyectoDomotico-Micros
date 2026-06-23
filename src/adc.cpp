// adc.cpp - Lectura ADC compartida (registros directos ATmega2560)

#include <avr/io.h>
#include "../include/adc.h"


// -- adc_init() ---------------------------------------------------------------
void adc_init() {

    // ADMUX: referencia AVCC (REFS0=1). El canal se fija en cada adc_leer().
    ADMUX = (1 << REFS0);

    // ADCSRB: MUX5=0 por defecto (canales 0-7). adc_leer() lo ajusta segun canal.
    ADCSRB = 0;

    // ADCSRA: habilitar ADC (ADEN) y prescaler 128 (ADPS2:0 = 111).
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

    // Conversion de calentamiento (la primera tras habilitar es poco fiable)
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    (void)ADC;
}


// -- adc_leer(canal) ----------------------------------------------------------
uint16_t adc_leer(uint8_t canal) {

    // Seleccionar el canal:
    //   MUX[2:0] (bits bajos de ADMUX) -> numero de canal dentro del grupo
    //   MUX5 (en ADCSRB) -> 1 para canales 8-15 (Puerto K), 0 para 0-7 (Puerto F)
    ADMUX = (1 << REFS0) | (canal & 0x07);
    if (canal >= 8) ADCSRB |= (1 << MUX5);
    else            ADCSRB &= ~(1 << MUX5);

    // Conversion de DESCARTE tras cambiar de canal: deja que el circuito de
    // muestreo y retencion (S/H) se cargue al nuevo canal. Sin esto, alternar
    // entre ADC9 (temp) y ADC0 (volumen) contamina las lecturas y hace que los
    // LEDs de calefactor/ventilador parpadeen.
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    (void)ADC;

    // Promediar 16 muestras para una lectura estable
    uint32_t suma = 0;
    for (uint8_t i = 0; i < 16; i++) {
        ADCSRA |= (1 << ADSC);
        while (ADCSRA & (1 << ADSC));
        suma += ADC;
    }

    return (uint16_t)(suma >> 4); // dividir entre 16
}

