#ifndef MOTOR_H
#define MOTOR_H

// motor.h - Servomotor del garaje (reemplaza al motor paso a paso)
// Sin librerias - registros directos ATmega2560
//
// El garaje ahora se controla con un SERVOMOTOR por PWM de 50 Hz (periodo
// 20 ms), variando el ancho de pulso:
//   ~1.0 ms  -> 0 grados   (garaje CERRADO)
//   ~2.0 ms  -> 180 grados (garaje ABIERTO)
//
// A diferencia del paso a paso, NO es bloqueante: se fija el ancho de pulso y
// el servo se mueve solo; el loop sigue corriendo (no se detienen el teclado,
// las alarmas ni el mercado).
//
// CONEXION FISICA:
//   Senal servo -> OC4A -> PH3 (pin D6)
//   - En PROTEUS: conectar la entrada del componente MOTOR-PWM / servo a D6.
//   - En el ENTRENADOR: conectar un LED + R220 a GND en D6 (el LED varia su
//     brillo segun la posicion: tenue = cerrado, mas brillante = abierto).
//
// Timer4 (16 bits) en Fast PWM, TOP=ICR4. Timer4 estaba libre (Timer1=dimmer,
// Timer2=teclado, Timer3=sonido).

#include <avr/io.h>

// -- Pin de la senal del servo (Puerto H, OC4A) ------------------------
#define SERVO_DDR    DDRH
#define SERVO_PIN    PH3   // OC4A, pin D6

// -- Anchos de pulso (en ticks del Timer4 con prescaler 8, 2 ticks = 1us) --
// Periodo 20 ms = 40000 ticks. ICR4 = 39999 (TOP).
#define SERVO_TOP            39999   // 50 Hz
#define SERVO_PULSO_CERRADO  2000    // ~1.0 ms -> 0 grados
#define SERVO_PULSO_ABIERTO  4000    // ~2.0 ms -> 180 grados

// -- Estados del garaje ----------------------------------------------------
#define GARAJE_CERRADO   0
#define GARAJE_ABIERTO   1

// -- API publica --------------------------------------------------------

// Configura Timer4 en Fast PWM 50 Hz sobre OC4A (PH3/D6) y deja el garaje
// CERRADO. Llamar en setup().
void motor_init();

// Abre el garaje: lleva el servo a la posicion ABIERTO (no bloqueante).
void garaje_abrir();

// Cierra el garaje: lleva el servo a la posicion CERRADO (no bloqueante).
void garaje_cerrar();

// Retorna el estado actual: GARAJE_CERRADO o GARAJE_ABIERTO
uint8_t garaje_estado();

#endif
