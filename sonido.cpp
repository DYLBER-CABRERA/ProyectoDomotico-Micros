// sonido.cpp - Control remoto del equipo de sonido
// Sin librerias - registros directos ATmega2560
// PWM en Timer3 canal A (OC3A, PE3, pin 5)

#include <avr/io.h>
#include "sonido.h"


static uint8_t estado_sonido   = SONIDO_APAGADO;
static uint8_t volumen_actual  = 0;


// -- sonido_init() -----------------------------------------------------------
void sonido_init() {

    // Configurar PH6 como salida (LED indicador)
    SONIDO_LED_DDR |= (1 << SONIDO_LED_PIN);
    PORTH &= ~(1 << SONIDO_LED_PIN); // apagado al inicio

    // Configurar PE3/OC3A como salida (PWM del volumen)
    SONIDO_PWM_DDR |= (1 << SONIDO_PWM_PIN);

    // -- Configurar Timer3 en modo Fast PWM de 8 bits, canal A --------
    //
    // TCCR3A: registro de control A del Timer3
    // COM3A1=1, COM3A0=0 -> modo no inversor en canal A (OC3A)
    // WGM30=1            -> junto con WGM32 (en TCCR3B), Fast PWM 8 bits
    TCCR3A = (1 << COM3A1) | (1 << WGM30);

    // TCCR3B: registro de control B del Timer3
    // WGM32=1 -> completa Fast PWM 8 bits
    // CS31=1  -> prescaler 8 (misma frecuencia PWM que el dimmer, ~7812 Hz)
    TCCR3B = (1 << WGM32) | (1 << CS31);

    // Iniciar con el volumen en 0 (silencio)
    OCR3A = 0;

    estado_sonido  = SONIDO_APAGADO;
    volumen_actual = 0;
}


// -- sonido_encender(volumen) -----------------------------------------------------------
void sonido_encender(uint8_t volumen) {

    // Limitar el volumen al rango valido 0-100
    if (volumen > 100) volumen = 100;

    volumen_actual = volumen;

    // Encender el LED indicador (equipo encendido)
    PORTH |= (1 << SONIDO_LED_PIN);

    // Convertir porcentaje (0-100) a valor de registro OCR3A (0-255)
    // uint16_t evita overflow durante la multiplicacion (100*255=25500)
    uint16_t valor_ocr = ((uint16_t)volumen * 255) / 100;

    OCR3A = (uint8_t)valor_ocr;

    estado_sonido = SONIDO_ENCENDIDO;
}


// -- sonido_apagar() -----------------------------------------------------------
void sonido_apagar() {

    // Apagar el LED indicador
    PORTH &= ~(1 << SONIDO_LED_PIN);

    // Poner el PWM en 0 (sin senal, silencio total)
    OCR3A = 0;

    estado_sonido = SONIDO_APAGADO;
    // NOTA: volumen_actual NO se borra -- asi sonido_volumen() puede
    // reportar cual era el ultimo volumen configurado, util para el
    // protocolo serial o para mostrar info en LCD aunque este apagado
}


// -- sonido_estado() -----------------------------------------------------------
uint8_t sonido_estado() {
    return estado_sonido;
}


// -- sonido_volumen() -----------------------------------------------------------
uint8_t sonido_volumen() {
    return volumen_actual;
}
