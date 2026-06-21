#ifndef MOTOR_H
#define MOTOR_H

// motor.h - Driver motor paso a paso (PAP) para el garaje
// Sin librerias - registros directos ATmega2560
//
// El motor PAP del entrenador tiene 4 bobinas. Se controla activando
// las bobinas en secuencia (NO con PWM como un servo). Cada paso
// completo de la secuencia gira el eje un angulo fijo (1.8 o 7.5
// grados segun el motor, verificar en el entrenador).
//
// CONEXION FISICA:
//   IN1 -> PG0 (pin 41)
//   IN2 -> PG1 (pin 40)
//   IN3 -> PG2 (pin 39)
//   IN4 -> PG3 (pin 38)
//   (el driver del motor en el entrenador ya maneja la corriente,
//    el Mega solo da las senales logicas de 5V)
//
// VELOCIDAD: se controla con el delay entre cada paso. Mas delay
// = mas lento pero con mas torque. Se usa Timer3 para el delay
// entre pasos sin bloquear el resto del sistema con _delay_ms largos.

#include <avr/io.h>

// -- Pines del motor (Puerto G) ----------------------------------------
#define MOTOR_DDR    DDRG
#define MOTOR_PORT   PORTG
#define MOTOR_IN1    PG0   // pin 41
#define MOTOR_IN2    PG1   // pin 40
#define MOTOR_IN3    PG2   // pin 39
#define MOTOR_IN4    PG3   // pin 38

// -- Direcciones de giro -------------------------------------------------
#define MOTOR_HORARIO      0   // abrir garaje
#define MOTOR_ANTIHORARIO  1   // cerrar garaje

// -- Cantidad de pasos para abrir/cerrar completamente el garaje --------
// Ajustar este valor experimentalmente en el entrenador fisico
// segun cuantos pasos necesita el motor para el recorrido completo
#define MOTOR_PASOS_GARAJE   512

// -- Estados del garaje ----------------------------------------------------
#define GARAJE_CERRADO   0
#define GARAJE_ABIERTO   1
#define GARAJE_MOVIENDO  2

// -- API publica --------------------------------------------------------

// Configura los 4 pines del motor como salidas. Llamar en setup().
void motor_init();

// Gira el motor N pasos en la direccion indicada.
// BLOQUEANTE por diseno: el garaje es una accion puntual (abrir/cerrar)
// que no necesita correr en paralelo con el resto del sistema.
// dir: MOTOR_HORARIO o MOTOR_ANTIHORARIO
void motor_pasos(uint16_t n, uint8_t dir);

// Detiene el motor: pone las 4 bobinas en LOW (sin energizar)
// Ahorra energia y evita calentamiento cuando no se esta moviendo
void motor_detener();

// Abre el garaje completamente (gira MOTOR_PASOS_GARAJE pasos horario)
// Actualiza el estado interno del garaje
void garaje_abrir();

// Cierra el garaje completamente (gira MOTOR_PASOS_GARAJE pasos antihorario)
void garaje_cerrar();

// Retorna el estado actual: GARAJE_CERRADO, GARAJE_ABIERTO o GARAJE_MOVIENDO
uint8_t garaje_estado();

#endif
