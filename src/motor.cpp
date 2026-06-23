// motor.cpp - Servomotor del garaje (PWM 50 Hz, Timer4 / OC4A / PH3)
// Sin librerias - registros directos ATmega2560

#include <avr/io.h>
#include "../include/motor.h"


// -- Estado interno del garaje -------------------------------------------
static uint8_t estado_garaje = GARAJE_CERRADO;


// -- motor_init() ---------------------------------------------------------
void motor_init() {

    // Pin de la senal del servo como salida (OC4A / PH3 / D6)
    SERVO_DDR |= (1 << SERVO_PIN);

    // -- Timer4 en Fast PWM con TOP = ICR4 (modo 14) -------------------
    // TCCR4A: COM4A1=1 -> PWM no inversor en OC4A. WGM41=1 (parte del modo 14).
    TCCR4A = (1 << COM4A1) | (1 << WGM41);

    // TCCR4B: WGM43=1, WGM42=1 (completan el modo 14). CS41=1 -> prescaler 8.
    //   f = 16MHz/8 = 2 MHz -> 2 ticks por microsegundo.
    TCCR4B = (1 << WGM43) | (1 << WGM42) | (1 << CS41);

    // TOP = 39999 -> periodo 40000 ticks = 20 ms = 50 Hz
    ICR4 = SERVO_TOP;

    // Posicion inicial: garaje CERRADO
    OCR4A = SERVO_PULSO_CERRADO;
    estado_garaje = GARAJE_CERRADO;
}


// -- garaje_abrir() -----------------------------------------------------------
// No bloqueante: solo cambia el ancho de pulso; el servo se mueve por su cuenta.
void garaje_abrir() {
    OCR4A = SERVO_PULSO_ABIERTO;
    estado_garaje = GARAJE_ABIERTO;
}


// -- garaje_cerrar() -----------------------------------------------------------
void garaje_cerrar() {
    OCR4A = SERVO_PULSO_CERRADO;
    estado_garaje = GARAJE_CERRADO;
}


// -- garaje_estado() -----------------------------------------------------------
uint8_t garaje_estado() {
    return estado_garaje;
}

