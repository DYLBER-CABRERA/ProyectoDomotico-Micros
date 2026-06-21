// dimmer.cpp - Control de iluminacion dimerizada via PWM (Timer1 canal A)
// Sin librerias - registros directos ATmega2560
// Rango ajustado a 0-10 por limitacion visual confirmada en Proteus

#include <avr/io.h>
#include "dimmer.h"


static uint8_t nivel_actual = 0;


// -- dimmer_init() -----------------------------------------------------------
void dimmer_init() {

    // Configurar PB5 (OC1A) como SALIDA. El modulo Compare del Timer1 necesita
    // que el pin sea salida para que la senal PWM generada internamente llegue
    // al exterior. Si el pin fuera entrada, el timer calcularia el duty cycle
    // pero no habria senal visible en el pin.
    DIMMER_DDR |= (1 << DIMMER_PIN);

    // TCCR1A: registro de control A del Timer1
    // COM1A1=1, COM1A0=0 → "Clear OC1A on Compare Match, Set at BOTTOM"
    //   = PWM no-invertido: OC1A sube a HIGH cuando el contador llega a 0 (BOTTOM)
    //     y baja a LOW cuando el contador iguala OCR1A.
    //     Resultado: duty cycle = OCR1A / 255
    // WGM10=1 → bit bajo del selector de modo de onda (WGM).
    //   Combinado con WGM12 en TCCR1B forma el modo 5 (Fast PWM 8-bit):
    //   TOP=0xFF=255, el contador cuenta 0→255 y se reinicia automaticamente.
    TCCR1A = (1 << COM1A1) | (1 << WGM10);

    // TCCR1B: registro de control B del Timer1
    // WGM12=1 → bit alto del selector WGM. Junto con WGM10=1 → modo 5 Fast PWM 8-bit.
    // CS11=1  → prescaler 8. Frecuencia de la PWM:
    //   f_PWM = F_CPU / (prescaler * (TOP+1)) = 16.000.000 / (8 * 256) ≈ 7812 Hz
    //   A ~7.8kHz el ojo humano no percibe parpadeo en el LED (umbral es ~50Hz).
    //   Usar prescaler=1 daria ~62kHz, innecesario; prescaler=64 daria ~977Hz, aceptable.
    TCCR1B = (1 << WGM12) | (1 << CS11);

    // OCR1A: Output Compare Register del canal A
    // En Fast PWM 8-bit: duty cycle = OCR1A / 255
    // OCR1A=0 → 0% duty cycle → LED completamente apagado al inicio del programa
    OCR1A = 0;
    nivel_actual = 0; // sincronizar la variable de nivel interno con OCR1A=0
}


// -- dimmer_set(nivel) -----------------------------------------------------------
// CAMBIO: nivel ahora va de 0 a 10 (DIMMER_NIVEL_MAX), no de 0 a 100.
// Esto concentra todo el rango util dentro de la zona donde el cambio
// de brillo SI es visible en la simulacion de Proteus (0% a ~8% duty).
void dimmer_set(uint8_t nivel) {

    if (nivel > DIMMER_NIVEL_MAX) nivel = DIMMER_NIVEL_MAX; // saturar: no superar el maximo

    nivel_actual = nivel; // guardar el nivel para que dimmer_get() lo pueda reportar

    // FORMULA ORIGINAL (abarca 0%-100% del duty cycle, rango completo):
    //   valor_ocr = (nivel * 255) / 10
    //   nivel=5 → OCR1A=127 (50% duty) → en Proteus el LED ya se ve igual que al 10%
    // Esta formula se calcula pero NO se asigna a OCR1A (se deja como referencia).
    // La variable queda sin usar intencionalmente para documentar la formula original.
    uint16_t valor_ocr = ((uint16_t)nivel * 255) / DIMMER_NIVEL_MAX; // referencia: NO se usa

    // FORMULA AJUSTADA PARA PROTEUS (rango visible 0% a ~7.8% duty):
    //   OCR1A = nivel * 2
    //   nivel=0  → OCR1A=0  → 0%   duty cycle → LED apagado
    //   nivel=5  → OCR1A=10 → 3.9% duty cycle → brillo medio
    //   nivel=10 → OCR1A=20 → 7.8% duty cycle → brillo maximo visible
    // En Proteus el brillo del LED no cambia visiblemente por encima del 10%
    // de duty cycle, por lo que comprimir el rango util a 0-20 en OCR1A
    // hace que cada nivel represente un cambio visible real en la simulacion.
    OCR1A = nivel * 2;
}


// -- dimmer_get() -----------------------------------------------------------
uint8_t dimmer_get() {
    return nivel_actual;
}
