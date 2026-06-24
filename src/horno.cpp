// horno.cpp - Control remoto del horno
// Sin librerias - registros directos ATmega2560

#include <avr/io.h>
#include "../include/horno.h"

// Reloj del sistema en ms (definido en teclado.cpp, base ISR Timer2 ~10ms).
extern uint32_t millis_sistema();


// -- Variables internas (privadas de este modulo) ----------------------
static uint8_t  estado_horno       = HORNO_APAGADO;
static uint8_t  temp_objetivo      = 0;
static uint16_t segundos_restantes = 0;

// Marca de tiempo (ms) del ultimo "tic" de 1 segundo. Se usa TIEMPO REAL
// (millis_sistema, base Timer2): 1000 ms = 1 segundo exacto, sin depender de
// la velocidad del loop.
static uint32_t ultimo_tick_horno = 0;


// -- horno_init() -----------------------------------------------------------
void horno_init() {

    // Configurar PH5 como salida (usar las macros del header, NO DDRE/PORTE).
    // PE5 esta ocupado por el sensor de incendio SW4/INT5: escribirlo aqui
    // rompia la alarma de incendio zona 2 (ver specs/defects/DEF-001).
    HORNO_DDR |= (1 << HORNO_PIN);

    // Apagar el LED al inicio
    HORNO_PORT &= ~(1 << HORNO_PIN);

    estado_horno       = HORNO_APAGADO;
    temp_objetivo       = 0;
    segundos_restantes  = 0;
    ultimo_tick_horno   = 0;
}


// -- horno_encender(temp_objetivo, segundos) ---------------------------------
void horno_encender(uint8_t temp, uint8_t segundos) {

    temp_objetivo = temp;

    // El parametro ahora son SEGUNDOS directamente (ej. 5 -> cuenta 5 segundos).
    segundos_restantes = segundos;

    // Encender el LED (simula activar el rele del horno)
    HORNO_PORT |= (1 << HORNO_PIN);

    estado_horno      = HORNO_ENCENDIDO;
    ultimo_tick_horno = millis_sistema(); // empezar a contar desde ahora
}


// -- horno_apagar() -----------------------------------------------------------
void horno_apagar() {

    HORNO_PORT &= ~(1 << HORNO_PIN);

    estado_horno       = HORNO_APAGADO;
    segundos_restantes = 0;
}


// -- horno_actualizar() -----------------------------------------------------------
// Llamar en cada vuelta del loop(). Avanza la cuenta regresiva y apaga
// el horno automaticamente al terminar.
uint8_t horno_actualizar() {

    // Si el horno esta apagado, no hay nada que actualizar
    if (estado_horno == HORNO_APAGADO) {
        return 0;
    }

    // Cada 1000 ms reales, restar 1 segundo de la cuenta regresiva.
    if (millis_sistema() - ultimo_tick_horno >= 1000) {
        ultimo_tick_horno = millis_sistema();

        if (segundos_restantes > 0) {
            segundos_restantes--;
        }

        // Si la cuenta llego a 0, apagar automaticamente
        if (segundos_restantes == 0) {
            horno_apagar();
            return 1; // el horno acaba de terminar en esta vuelta
        }
    }

    return 0; // nada nuevo que reportar en esta vuelta
}


// -- horno_estado() -----------------------------------------------------------
uint8_t horno_estado() {
    return estado_horno;
}


// -- horno_segundos_restantes() -----------------------------------------------------------
uint16_t horno_segundos_restantes() {
    return segundos_restantes;
}


// -- horno_temp_objetivo() -----------------------------------------------------------
uint8_t horno_temp_objetivo() {
    return temp_objetivo;
}

