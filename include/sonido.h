#ifndef SONIDO_H
#define SONIDO_H

// sonido.h - Control remoto del equipo de sonido (encendido + volumen)
// Sin librerias - registros directos ATmega2560
//
// El volumen llega por comando serial (0-100) y se convierte en PWM.
// El PWM pasa por un filtro RC (fuera del micro, en el circuito fisico)
// que lo convierte en un voltaje analogico proporcional al volumen --
// asi lo exige el enunciado: "senal analogica proporcional al volumen".
//
// CONEXION FISICA:
//   Pot. volumen    -> ADC13 (PK5 / A13)    [entrada analogica del usuario]
//   PWM volumen     -> OC3A -> PE3 (pin 5)  [+ filtro RC -> senal analogica de salida]
//   LED equipo ON   -> PH6 (pin 9) + R220 a GND
//
// El VOLUMEN lo fija el potenciometro en A13 en tiempo real mientras el equipo
// esta encendido (ver sonido_actualizar()). El encendido/apagado es por comando
// (teclado *21/*20 o serial SONIDO:ON / SONIDO:OFF).
//
// COMANDOS: SONIDO:ON  /  SONIDO:OFF   (el volumen ya no se pasa por comando)

#include <avr/io.h>

#define SONIDO_LED_DDR   DDRH
#define SONIDO_LED_PIN   PH6   // pin 9, indicador de equipo encendido

#define SONIDO_PWM_DDR   DDRE
#define SONIDO_PWM_PIN   PE3   // pin 5, OC3A (Timer3 canal A)

#define SONIDO_POT_CANAL 13    // ADC13 (PK5 / A13): potenciometro de volumen

// Estados
#define SONIDO_APAGADO   0
#define SONIDO_ENCENDIDO 1

// -- API publica --------------------------------------------------------

// Configura Timer3 en Fast PWM para el volumen y el pin del LED indicador.
// Llamar en setup().
void sonido_init();

// Enciende el equipo de sonido. Enciende el LED indicador; a partir de aqui
// el volumen lo controla el potenciometro (ver sonido_actualizar()).
// El parametro se mantiene por compatibilidad pero se ignora (el pot manda).
void sonido_encender(uint8_t volumen_ignorado);

// Debe llamarse en cada vuelta del loop(). Si el equipo esta encendido, lee el
// potenciometro de volumen (ADC0) y actualiza el PWM en tiempo real.
void sonido_actualizar();

// Apaga el equipo: apaga el LED y pone el PWM en 0 (silencio)
void sonido_apagar();

// Retorna el estado actual: SONIDO_APAGADO o SONIDO_ENCENDIDO
uint8_t sonido_estado();

// Retorna el volumen actual configurado (0-100)
uint8_t sonido_volumen();

#endif
