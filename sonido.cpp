// sonido.cpp - Control remoto del equipo de sonido
// Sin librerias - registros directos ATmega2560
// PWM en Timer3 canal A (OC3A, PE3, pin 5)

#include <avr/io.h>
#include "sonido.h"
#include "adc.h"   // lectura del potenciometro de volumen (ADC13 / PK5 / A13)


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


// -- sonido_encender() -----------------------------------------------------------
// Enciende el equipo. El volumen NO se fija aqui: lo controla el potenciometro
// en tiempo real (ver sonido_actualizar()). El parametro se ignora.
void sonido_encender(uint8_t volumen_ignorado) {

    (void)volumen_ignorado; // el volumen lo decide el potenciometro (ADC0)

    // Encender el LED indicador (equipo encendido)
    PORTH |= (1 << SONIDO_LED_PIN);

    estado_sonido = SONIDO_ENCENDIDO;

    // Aplicar de inmediato el volumen actual del potenciometro
    sonido_actualizar();
}


// -- sonido_actualizar() -----------------------------------------------------------
// Llamar en cada vuelta del loop(). Si el equipo esta encendido, lee el
// potenciometro de volumen (ADC13 / PK5 / A13, 0-1023) y lo traduce a:
//   - volumen_actual: porcentaje 0-100 (para reportes)
//   - OCR3A:          duty 0-255 del PWM -> tras el filtro RC, la senal
//                     analogica proporcional al volumen que exige el enunciado.
void sonido_actualizar() {

    if (estado_sonido != SONIDO_ENCENDIDO) return; // apagado: nada que hacer

    uint16_t lectura = adc_leer(SONIDO_POT_CANAL); // 0-1023 del potenciometro

    // Porcentaje 0-100 para reportar por serial/LCD
    volumen_actual = (uint8_t)(((uint32_t)lectura * 100) / 1023);

    // Duty 0-255 directamente proporcional a la lectura (1023 -> 255)
    OCR3A = (uint8_t)(((uint32_t)lectura * 255) / 1023);
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
