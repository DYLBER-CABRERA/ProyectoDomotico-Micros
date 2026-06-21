// motor.cpp - Driver motor paso a paso (PAP)
// Sin librerias - registros directos ATmega2560

#include <avr/io.h>
#include <util/delay.h>
#include "motor.h"


// -- Estado interno del garaje -------------------------------------------
static uint8_t estado_garaje = GARAJE_CERRADO;


// -- secuencia_paso(paso) -------------------------------------------------
// Funcion privada. Activa las bobinas correspondientes a un paso
// especifico (0 a 3) de la secuencia de 4 pasos.
// Secuencia de paso completo de a 1 bobina activa por vez:
//   paso 0: IN1=1 IN2=0 IN3=0 IN4=0
//   paso 1: IN1=0 IN2=1 IN3=0 IN4=0
//   paso 2: IN1=0 IN2=0 IN3=1 IN4=0
//   paso 3: IN1=0 IN2=0 IN3=0 IN4=1
static void secuencia_paso(uint8_t paso) {

    // Limpiar las 4 bobinas primero (todas en 0)
    // &= ~(...) -> apaga solo los 4 bits del motor, no toca el resto
    // de Puerto G por si tiene otro uso
    MOTOR_PORT &= ~((1 << MOTOR_IN1) | (1 << MOTOR_IN2) |
                    (1 << MOTOR_IN3) | (1 << MOTOR_IN4));

    // Activar SOLO la bobina que corresponde a este paso
    switch (paso) {
        case 0: MOTOR_PORT |= (1 << MOTOR_IN1); break;
        case 1: MOTOR_PORT |= (1 << MOTOR_IN2); break;
        case 2: MOTOR_PORT |= (1 << MOTOR_IN3); break;
        case 3: MOTOR_PORT |= (1 << MOTOR_IN4); break;
    }
}


// -- motor_init() ---------------------------------------------------------
void motor_init() {

    // Configurar los 4 pines del motor como SALIDAS
    // |= -> solo afecta esos 4 bits de Puerto G
    MOTOR_DDR |= (1 << MOTOR_IN1) | (1 << MOTOR_IN2) |
                 (1 << MOTOR_IN3) | (1 << MOTOR_IN4);

    // Iniciar con el motor detenido (todas las bobinas apagadas)
    motor_detener();

    estado_garaje = GARAJE_CERRADO;
}


// -- motor_pasos(n, dir) ----------------------------------------------------
// Gira el motor n pasos en la direccion indicada.
// BLOQUEANTE: usa _delay_ms entre cada paso. Aceptable porque
// abrir/cerrar el garaje es una accion puntual que no compite
// con otras tareas urgentes (alarmas siguen funcionando por
// interrupcion incluso durante este bloqueo).
void motor_pasos(uint16_t n, uint8_t dir) {

    // paso_actual recorre 0,1,2,3,0,1,2,3... (secuencia ciclica)
    static uint8_t paso_actual = 0;

    for (uint16_t i = 0; i < n; i++) {

        if (dir == MOTOR_HORARIO) {
            // Avanzar la secuencia: 0->1->2->3->0...
            paso_actual = (paso_actual + 1) % 4;
        } else {
            // Retroceder la secuencia: 3->2->1->0->3...
            // Sumar 3 y aplicar modulo 4 es equivalente a restar 1
            // de forma segura sin numeros negativos en uint8_t
            paso_actual = (paso_actual + 3) % 4;
        }

        secuencia_paso(paso_actual);

        // Delay entre pasos - controla la VELOCIDAD del motor
        // Mas delay = mas lento pero mas torque (menos vibracion)
        // Ajustar este valor segun el motor especifico del entrenador
        _delay_ms(5);
    }
}


// -- motor_detener() --------------------------------------------------------
void motor_detener() {
    // Apagar las 4 bobinas - el motor queda libre, sin consumir
    // corriente ni generar torque de retencion
    MOTOR_PORT &= ~((1 << MOTOR_IN1) | (1 << MOTOR_IN2) |
                    (1 << MOTOR_IN3) | (1 << MOTOR_IN4));
}


// -- garaje_abrir() -----------------------------------------------------------
void garaje_abrir() {

    estado_garaje = GARAJE_MOVIENDO;

    motor_pasos(MOTOR_PASOS_GARAJE, MOTOR_HORARIO);

    motor_detener(); // ahorra energia al terminar el movimiento

    estado_garaje = GARAJE_ABIERTO;
}


// -- garaje_cerrar() -----------------------------------------------------------
void garaje_cerrar() {

    estado_garaje = GARAJE_MOVIENDO;

    motor_pasos(MOTOR_PASOS_GARAJE, MOTOR_ANTIHORARIO);

    motor_detener();

    estado_garaje = GARAJE_CERRADO;
}


// -- garaje_estado() -----------------------------------------------------------
uint8_t garaje_estado() {
    return estado_garaje;
}
