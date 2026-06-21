// alarma.cpp - Maquina de estados de alarmas (acceso + incendio)
// Sin librerias - registros directos ATmega2560

#include <avr/io.h>
#include <avr/interrupt.h>
#include "alarma.h"


// -- Variables internas (privadas de este modulo) ----------------------
// volatile: modificadas en ISR, leidas en el loop principal

static volatile uint8_t estado_acceso     = ALARMA_DESACTIVADA;
static volatile uint8_t bandera_incendio  = 0;  // 1 = sensor humo activo
static volatile uint8_t bandera_acceso    = 0;  // 1 = sensor puerta/vent activo
static volatile uint8_t zona_disparo      = 0;  // que zona especifica disparo
static volatile uint8_t tipo_disparo_actual = ALARMA_TIPO_NINGUNO;

// Codigo de armado/desarmado por defecto (ajustar segun lo acordado)
// static const -> en Flash, no se puede modificar en tiempo de ejecucion
static const char codigo_alarma[ALARMA_CODIGO_LEN + 1] = "1234";


// -- ISR de INCENDIO (SIEMPRE activas, no requieren armado) -------------
// REGLA DEL PROYECTO: ISR minima -- solo banderas, nada de LCD/USART aqui

ISR(INT4_vect) {
    // SW3 del DIPSW -> zona de humo 1 (ej. cocina)
    bandera_incendio = 1;
    zona_disparo     = 1;
}

ISR(INT5_vect) {
    // SW4 del DIPSW -> zona de humo 2 (ej. garaje)
    bandera_incendio = 1;
    zona_disparo     = 2;
}


// -- ISR de ACCESO (solo disparan si la alarma esta ARMADA) -------------
// Aunque el hardware siempre puede generar la interrupcion una vez
// habilitada en EIMSK, aqui se filtra por software: si no esta armada,
// la bandera de acceso se ignora en alarma_actualizar()

ISR(INT2_vect) {
    // SW1 del DIPSW -> puerta principal
    bandera_acceso = 1;
    zona_disparo   = 3;
}

ISR(INT3_vect) {
    // SW2 del DIPSW -> ventana / garaje
    bandera_acceso = 1;
    zona_disparo   = 4;
}


// -- alarma_init() ---------------------------------------------------------
// Configura LEDs, registros de interrupcion, y deja el incendio
// SIEMPRE vigilando desde el arranque (segun decision de diseno)
void alarma_init() {

    // -- 1. Configurar LEDs como salidas (Puerto B) --------------------
    // |= -> OR: solo afecta esos 2 bits, no toca el resto de PORTB
    // (PB0-PB3 los usa el SPI/RFID en otras fases, no se deben tocar)
    DDRB |= (1 << LED_DISPARADA) | (1 << LED_ARMADA);

    // Apagar ambos LEDs al inicio
    PORTB &= ~((1 << LED_DISPARADA) | (1 << LED_ARMADA));

    // -- 2. Configurar EICRA: flanco de subida para INT2,INT3,INT4,INT5

    // EICRA controla INT0-INT3 (registro A)
    // ISC21=1,ISC20=1 -> INT2 dispara en flanco de SUBIDA (LOW->HIGH)
    // ISC31=1,ISC30=1 -> INT3 dispara en flanco de SUBIDA
    // (flanco de subida porque con R10K a GND, el pin esta en LOW
    //  en reposo y sube a HIGH cuando el switch cierra)
    EICRA = (1 << ISC21) | (1 << ISC20) | (1 << ISC31) | (1 << ISC30);

    // EICRB controla INT4-INT7 (registro B)
    // ISC41=1,ISC40=1 -> INT4 dispara en flanco de SUBIDA
    // ISC51=1,ISC50=1 -> INT5 dispara en flanco de SUBIDA
    EICRB = (1 << ISC41) | (1 << ISC40) | (1 << ISC51) | (1 << ISC50);

    // -- 3. Habilitar en EIMSK SOLO las de incendio (INT4, INT5) -------
    // El incendio debe vigilar SIEMPRE desde el arranque, sin importar
    // si la alarma de acceso esta armada o no.
    // Las de acceso (INT2, INT3) se habilitan despues, en alarma_armar()
    EIMSK = (1 << INT4) | (1 << INT5);

    // -- 4. Estado inicial ----------------------------------------------
    estado_acceso        = ALARMA_DESACTIVADA;
    bandera_incendio     = 0;
    bandera_acceso       = 0;
    tipo_disparo_actual  = ALARMA_TIPO_NINGUNO;
}


// -- alarma_armar() ----------------------------------------------------------
// Arma SOLO la alarma de acceso. El incendio ya esta activo desde init().
void alarma_armar() {

    estado_acceso = ALARMA_ARMADA;

    // Encender LED verde (armada)
    PORTB |= (1 << LED_ARMADA);

    // Habilitar las interrupciones de acceso (INT2, INT3)
    // |= -> se suman a las que ya estaban (INT4, INT5 de incendio)
    // sin apagarlas
    EIMSK |= (1 << INT2) | (1 << INT3);

    // Limpiar cualquier bandera de acceso vieja que pudiera quedar
    bandera_acceso = 0;
}


// -- alarma_desarmar() --------------------------------------------------------
// Desarma el acceso y silencia cualquier disparo activo (de cualquier tipo)
void alarma_desarmar() {

    estado_acceso = ALARMA_DESACTIVADA;

    // Apagar LED verde (ya no esta armada)
    PORTB &= ~(1 << LED_ARMADA);

    // Apagar LED rojo (silenciar disparo, si habia uno)
    PORTB &= ~(1 << LED_DISPARADA);

    // Deshabilitar SOLO las interrupciones de acceso (INT2, INT3)
    // &= ~ -> apaga esos bits sin tocar INT4/INT5 (incendio sigue activo)
    EIMSK &= ~((1 << INT2) | (1 << INT3));

    // Limpiar banderas y tipo de disparo
    bandera_acceso      = 0;
    bandera_incendio    = 0;
    tipo_disparo_actual = ALARMA_TIPO_NINGUNO;
}


// -- alarma_actualizar() -------------------------------------------------------
// Se llama en cada vuelta del loop(). Revisa las banderas que las
// ISR dejaron y actualiza el estado y los LEDs.
// Retorna 1 si hubo un evento NUEVO que reportar en esta vuelta.
uint8_t alarma_actualizar() {

    // -- Caso 1: disparo por INCENDIO (siempre se revisa, sin importar
    //            si la alarma de acceso esta armada o no) --------------
    if (bandera_incendio) {

        bandera_incendio    = 0;       // consumir la bandera
        tipo_disparo_actual = ALARMA_TIPO_INCENDIO;

        // Encender LED rojo
        PORTB |= (1 << LED_DISPARADA);

        return 1; // hay evento nuevo para reportar
    }

    // -- Caso 2: disparo por ACCESO (solo cuenta si esta ARMADA) -------
    if (bandera_acceso) {

        bandera_acceso = 0; // consumir la bandera siempre, este armada o no

        if (estado_acceso == ALARMA_ARMADA) {

            estado_acceso       = ALARMA_DISPARADA;
            tipo_disparo_actual = ALARMA_TIPO_ACCESO;

            // Encender LED rojo
            PORTB |= (1 << LED_DISPARADA);

            return 1; // hay evento nuevo para reportar
        }
        // Si no estaba armada, se ignora el movimiento del switch
        // (comportamiento esperado: sin armar, acceso no dispara nada)
    }

    return 0; // nada nuevo en esta vuelta del loop
}


// -- alarma_estado() ------------------------------------------------------------
uint8_t alarma_estado() {
    return estado_acceso;
}


// -- alarma_tipo_disparo() ------------------------------------------------------
uint8_t alarma_tipo_disparo() {
    return tipo_disparo_actual;
}


// -- alarma_verificar_codigo(codigo_ingresado) -----------------------------------
// Compara byte a byte el codigo ingresado contra el codigo guardado
uint8_t alarma_verificar_codigo(const char* codigo_ingresado) {

    for (uint8_t i = 0; i < ALARMA_CODIGO_LEN; i++) {
        if (codigo_ingresado[i] != codigo_alarma[i]) {
            return 0; // algun digito no coincide
        }
    }
    return 1; // todos los digitos coinciden
}
