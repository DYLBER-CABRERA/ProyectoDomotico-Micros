#ifndef HORNO_H
#define HORNO_H

// horno.h - Control remoto del horno (tiempo + temperatura)
// Sin librerias - registros directos ATmega2560
//
// La temperatura del horno es un VALOR RECIBIDO por comando, no medido
// con sensor ADC propio. El LED simula el rele que enciende el horno.
//
// CAMBIO DE PIN: PE5 estaba ocupado por SW4 del DIPSW (alarma de
// incendio zona 2, INT5). Se mueve el LED del horno a PH5 (pin 8),
// que esta libre (Timer4 canal C, OC4C, sin uso actual en PWM).
//
// CONEXION FISICA:
//   LED horno (simula rele) -> PH5 (pin 8) + R220 a GND
//
// COMANDO: HORNO:<temp_C>,<minutos>  ej. HORNO:180,25

#include <avr/io.h>

#define HORNO_DDR    DDRH
#define HORNO_PORT   PORTH
#define HORNO_PIN    PH5

// Estados del horno
#define HORNO_APAGADO     0
#define HORNO_ENCENDIDO   1

// -- API publica --------------------------------------------------------

// Configura el pin del LED del horno como salida. Llamar en setup().
void horno_init();

// Enciende el horno: enciende el LED, guarda la temperatura objetivo
// y arranca la cuenta regresiva de 'minutos' minutos.
void horno_encender(uint8_t temp_objetivo, uint8_t minutos);

// Apaga el horno inmediatamente (LED apagado, cuenta regresiva cancelada)
void horno_apagar();

// Debe llamarse en cada vuelta del loop(). Avanza la cuenta regresiva
// y apaga el horno automaticamente al llegar a 0. Retorna 1 si el horno
// acaba de terminar en esta vuelta (para reportar por USART/LCD), 0 si no.
uint8_t horno_actualizar();

// Retorna el estado actual: HORNO_APAGADO o HORNO_ENCENDIDO
uint8_t horno_estado();

// Retorna los segundos restantes de la cuenta regresiva actual
uint16_t horno_segundos_restantes();

// Retorna la temperatura objetivo configurada en el ultimo horno_encender()
uint8_t horno_temp_objetivo();

#endif
