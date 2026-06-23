#ifndef SONIDO_H
#define SONIDO_H

// sonido.h - Control remoto del equipo de sonido (encendido + volumen)
// Sin librerias - registros directos ATmega2560
//
// El volumen llega por comando serial (escala 0-10) y se convierte en PWM
// con curva gamma perceptual. El PWM pasa por un filtro RC (fuera del micro)
// que lo convierte en voltaje analogico proporcional al volumen -- cumple
// el enunciado: "senal analogica proporcional al volumen solicitado".
//
// CONEXION FISICA:
//   PWM volumen  -> OC3A -> PE3 (pin 5) + filtro RC -> senal analogica de salida
//   LED equipo   -> PH6 (pin 9) + R220 a GND
//
// COMANDOS: SONIDO:ON [,N]  /  SONIDO:OFF  /  VOL:N  /  VOL:+  /  VOL:-

#include <avr/io.h>

#define SONIDO_LED_DDR   DDRH
#define SONIDO_LED_PIN   PH6   // pin 9, indicador de equipo encendido

#define SONIDO_PWM_DDR   DDRE
#define SONIDO_PWM_PIN   PE3   // pin 5, OC3A (Timer3 canal A)

#define SONIDO_VOL_MAX   10    // escala 0-10 (igual que el dimmer)

#define SONIDO_APAGADO   0
#define SONIDO_ENCENDIDO 1

// -- API publica --------------------------------------------------------

// Configura Timer3 en Fast PWM para el volumen y el pin del LED.
// Llamar en setup().
void sonido_init();

// Enciende el equipo al nivel indicado (0-10). Activa el LED y aplica el PWM.
void sonido_encender(uint8_t nivel);

// Cambia el volumen en tiempo real (0-10). Si el equipo esta apagado,
// guarda el nivel como preset para el proximo encendido.
void sonido_set_volumen(uint8_t nivel);

// Apaga el equipo: LED apagado, PWM en 0 (silencio).
void sonido_apagar();

// Stub de compatibilidad con el loop() -- ya no hace nada (volumen por serial).
void sonido_actualizar();

// Retorna el estado actual: SONIDO_APAGADO o SONIDO_ENCENDIDO.
uint8_t sonido_estado();

// Retorna el nivel de volumen actual (0-10).
uint8_t sonido_volumen();

#endif
