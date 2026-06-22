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
//   PWM volumen     -> OC3A -> PE3 (pin 5)  [+ filtro RC en el circuito]
//   LED equipo ON   -> PH6 (pin 9) + R220 a GND
//
// COMANDOS: SONIDO:ON,<0-100>  /  SONIDO:OFF

#include <avr/io.h>

#define SONIDO_LED_DDR   DDRH
#define SONIDO_LED_PIN   PH6   // pin 9, indicador de equipo encendido

#define SONIDO_PWM_DDR   DDRE
#define SONIDO_PWM_PIN   PE3   // pin 5, OC3A (Timer3 canal A)

// Estados
#define SONIDO_APAGADO   0
#define SONIDO_ENCENDIDO 1

// -- API publica --------------------------------------------------------

// Configura Timer3 en Fast PWM para el volumen y el pin del LED indicador.
// Llamar en setup().
void sonido_init();

// Enciende el equipo de sonido con el volumen indicado (0-100).
// Enciende el LED indicador y ajusta el duty cycle del PWM.
void sonido_encender(uint8_t volumen);

// Apaga el equipo: apaga el LED y pone el PWM en 0 (silencio)
void sonido_apagar();

// Retorna el estado actual: SONIDO_APAGADO o SONIDO_ENCENDIDO
uint8_t sonido_estado();

// Retorna el volumen actual configurado (0-100)
uint8_t sonido_volumen();

#endif
