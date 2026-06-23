// sonido.cpp - Control remoto del equipo de sonido
// Sin librerias - registros directos ATmega2560
// PWM en Timer3 canal A (OC3A, PE3, pin 5)
// Volumen controlado por comandos serial (VOL:N / VOL:+ / VOL:-)

#include <avr/io.h>
#include "../include/sonido.h"


static uint8_t estado_sonido  = SONIDO_APAGADO;
static uint8_t volumen_actual = 0;


// Curva gamma perceptual: el oido percibe el volumen de forma logaritmica.
// Cada paso multiplica aprox. x1.7 el duty anterior para que el cambio
// de volumen se sienta parejo entre niveles (igual que el dimmer).
static const uint8_t curva_vol[SONIDO_VOL_MAX + 1] = {
    0,    // 0  -> silencio
    2,    // 1
    4,    // 2
    8,    // 3
    14,   // 4
    25,   // 5
    44,   // 6
    78,   // 7
    124,  // 8
    180,  // 9
    255   // 10 -> volumen maximo
};


// -- sonido_init() -----------------------------------------------------------
void sonido_init() {

    SONIDO_LED_DDR |= (1 << SONIDO_LED_PIN);
    PORTH &= ~(1 << SONIDO_LED_PIN); // LED apagado al inicio

    SONIDO_PWM_DDR |= (1 << SONIDO_PWM_PIN);

    // Timer3 Fast PWM 8 bits, canal A, prescaler 8 (~7812 Hz)
    TCCR3A = (1 << COM3A1) | (1 << WGM30);
    TCCR3B = (1 << WGM32) | (1 << CS31);
    OCR3A  = 0;

    estado_sonido  = SONIDO_APAGADO;
    volumen_actual = 0;
}


// -- sonido_encender(nivel) --------------------------------------------------
void sonido_encender(uint8_t nivel) {

    if (nivel > SONIDO_VOL_MAX) nivel = SONIDO_VOL_MAX;

    volumen_actual = nivel;
    OCR3A = curva_vol[nivel];

    PORTH |= (1 << SONIDO_LED_PIN);
    estado_sonido = SONIDO_ENCENDIDO;
}


// -- sonido_set_volumen(nivel) -----------------------------------------------
// Cambia el nivel de volumen en tiempo real.
// Si el equipo esta apagado, guarda el preset pero mantiene OCR3A en 0.
void sonido_set_volumen(uint8_t nivel) {

    if (nivel > SONIDO_VOL_MAX) nivel = SONIDO_VOL_MAX;

    volumen_actual = nivel;

    if (estado_sonido == SONIDO_ENCENDIDO) {
        OCR3A = curva_vol[nivel];
    }
}


// -- sonido_apagar() -----------------------------------------------------------
void sonido_apagar() {

    PORTH &= ~(1 << SONIDO_LED_PIN);
    OCR3A = 0;
    estado_sonido = SONIDO_APAGADO;
    // volumen_actual se conserva como preset para el proximo encendido
}


// -- sonido_actualizar() -----------------------------------------------------
// Stub vacio: el volumen ahora lo controla el comando serial, no el ADC.
// Se mantiene para no romper la llamada en loop().
void sonido_actualizar() {}


// -- sonido_estado() / sonido_volumen() --------------------------------------
uint8_t sonido_estado()  { return estado_sonido;  }
uint8_t sonido_volumen() { return volumen_actual;  }
