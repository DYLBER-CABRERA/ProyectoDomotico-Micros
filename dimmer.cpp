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

    // TABLA DE BRILLO PERCEPTUAL (curva logaritmica/gamma).
    // El duty crece de forma ~exponencial para que el cambio de brillo se vea
    // PAREJO entre niveles (el ojo percibe el brillo de forma logaritmica).
    //   indice = nivel (0-10) -> valor de OCR1A (0-255 = 0%-100% de duty)
    // Cada paso multiplica aprox. x1.7 el anterior. Ajustable a gusto.
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

    OCR1A = curva[nivel]; // aplicar el valor de la curva al duty cycle
}


// -- dimmer_get() -----------------------------------------------------------
uint8_t dimmer_get() {
    return nivel_actual;
}
