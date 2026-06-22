// horno.cpp - Control remoto del horno
// Sin librerias - registros directos ATmega2560

#include <avr/io.h>
#include "horno.h"


// -- Variables internas (privadas de este modulo) ----------------------
static uint8_t  estado_horno       = HORNO_APAGADO;
static uint8_t  temp_objetivo      = 0;
static uint16_t segundos_restantes = 0;

// Contador de vueltas del loop para convertir "vueltas" en "segundos".
// Mismo patron que usa temperatura.cpp con INTERVALO_TEMP, pero aqui
// el intervalo representa 1 segundo real (ajustar segun velocidad del loop).
static uint16_t contador_segundo = 0;
#define VUELTAS_POR_SEGUNDO  4000  // ajustar experimentalmente si la cuenta va muy rapida/lenta


// -- horno_init() -----------------------------------------------------------
void horno_init() {

    // Configurar PE5 como salida
    DDRE |= (1 << HORNO_PIN);

    // Apagar el LED al inicio
    PORTE &= ~(1 << HORNO_PIN);

    estado_horno       = HORNO_APAGADO;
    temp_objetivo       = 0;
    segundos_restantes  = 0;
    contador_segundo    = 0;
}


// -- horno_encender(temp_objetivo, minutos) ----------------------------------
void horno_encender(uint8_t temp, uint8_t minutos) {

    temp_objetivo = temp;

    // Convertir minutos a segundos para la cuenta regresiva interna
    // uint16_t evita overflow: 255 min * 60 = 15300, cabe en 16 bits
    segundos_restantes = (uint16_t)minutos * 60;

    // Encender el LED (simula activar el rele del horno)
    PORTE |= (1 << HORNO_PIN);

    estado_horno    = HORNO_ENCENDIDO;
    contador_segundo = 0; // reiniciar el contador de tiempo
}


// -- horno_apagar() -----------------------------------------------------------
void horno_apagar() {

    PORTE &= ~(1 << HORNO_PIN);

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

    contador_segundo++;

    // Cada VUELTAS_POR_SEGUNDO vueltas del loop, restar 1 segundo real
    if (contador_segundo >= VUELTAS_POR_SEGUNDO) {
        contador_segundo = 0;

        if (segundos_restantes > 0) {
            segundos_restantes--;
        }

        // Si la cuenta llego a 0, apagar automaticamente
        if (segundos_restantes == 0) {
            horno_apagar();
            return 1; // avisar que el horno acaba de terminar en esta vuelta
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
