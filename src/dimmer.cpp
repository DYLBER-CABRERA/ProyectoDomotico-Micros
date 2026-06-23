// dimmer.cpp - Control de iluminacion dimerizada via PWM (Timer1 canal A)
// Sin librerias - registros directos ATmega2560
// Rango ajustado a 0-10 por limitacion visual confirmada en Proteus

#include <avr/io.h>
#include "../include/dimmer.h"


static uint8_t nivel_actual = 0;


// -- dimmer_init() -----------------------------------------------------------
void dimmer_init() {

    DIMMER_DDR  |= (1 << DIMMER_PIN);
    DIMMER_PORT &= ~(1 << DIMMER_PIN); // pin LOW desde el inicio (nivel 0 = apagado real)

    // TCCR1A: WGM10=1 activa Fast PWM 8-bit (junto con WGM12 en TCCR1B).
    // COM1A1 empieza en 0 (OC1A desconectado del timer) porque nivel=0 y el
    // Fast PWM con OCR1A=0 genera un pulso estrecho (~1 ciclo) que hace brillar
    // levemente el LED. Al desconectar el pin del timer se elimina ese pulso.
    TCCR1A = (1 << WGM10); // COM1A1=0: OC1A desconectado, pin queda en LOW

    // Fast PWM 8-bit, prescaler 8 → ~7812 Hz (sin parpadeo visible)
    TCCR1B = (1 << WGM12) | (1 << CS11);

    OCR1A = 0;
    nivel_actual = 0;
}


// -- dimmer_set(nivel) -----------------------------------------------------------
// CAMBIO: nivel ahora va de 0 a 10 (DIMMER_NIVEL_MAX), no de 0 a 100.
// Esto concentra todo el rango util dentro de la zona donde el cambio
// de brillo SI es visible en la simulacion de Proteus (0% a ~8% duty).
void dimmer_set(uint8_t nivel) {

    if (nivel > DIMMER_NIVEL_MAX) nivel = DIMMER_NIVEL_MAX;

    nivel_actual = nivel;

    static const uint8_t curva[DIMMER_NIVEL_MAX + 1] = {
        0,    // 0  -> apagado
        2,    // 1
        4,    // 2
        8,    // 3
        14,   // 4
        25,   // 5
        44,   // 6
        78,   // 7
        124,  // 8
        180,  // 9
        255   // 10 -> brillo maximo
    };

    if (nivel == 0) {
        // Desconectar OC1A del timer (COM1A1=0) y forzar pin a LOW.
        // Con OCR1A=0 en Fast PWM no-inversor el ATmega2560 genera un pulso
        // estrecho (~1 ciclo de reloj) en cada periodo que hace brillar el LED.
        // La unica forma de evitarlo es desconectar el pin del modulo compare.
        TCCR1A &= ~(1 << COM1A1);
        DIMMER_PORT &= ~(1 << DIMMER_PIN); // pin LOW = LED completamente apagado
        OCR1A = 0;
    } else {
        // Reconectar OC1A al timer (COM1A1=1) y aplicar el duty cycle de la curva
        TCCR1A |= (1 << COM1A1);
        OCR1A = curva[nivel];
    }
}


// -- dimmer_get() -----------------------------------------------------------
uint8_t dimmer_get() {
    return nivel_actual;
}

