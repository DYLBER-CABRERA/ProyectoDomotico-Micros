#include "../include/acceso.h"
#include "../include/motor.h"

// -- acceso_init() ---------------------------------------------------------
void acceso_init() {
    // Configurar PG0 como salida (iman/relé de la puerta principal)
    IMAN_DDR |= (1 << IMAN_PIN);
    // Iman apagado por defecto (puerta cerrada)
    IMAN_PORT &= ~(1 << IMAN_PIN);
}

// -- acceso_abrir_principal() ---------------------------------------------
void acceso_abrir_principal() {
    IMAN_PORT |= (1 << IMAN_PIN);
}

// -- acceso_cerrar_principal() --------------------------------------------
void acceso_cerrar_principal() {
    IMAN_PORT &= ~(1 << IMAN_PIN);
}

// -- acceso_abrir_garaje() ------------------------------------------------
void acceso_abrir_garaje() {
    garaje_abrir();
}

// -- acceso_cerrar_garaje() -----------------------------------------------
void acceso_cerrar_garaje() {
    garaje_cerrar();
}

// -- acceso_concedido_adulto() --------------------------------------------
void acceso_concedido_adulto() {
    acceso_abrir_principal();
}

// -- acceso_concedido_hijo() ----------------------------------------------
void acceso_concedido_hijo() {
    acceso_abrir_principal();
}

// -- acceso_denegado() ----------------------------------------------------
void acceso_denegado() {
    // Sin accion fisica; el modulo rfid envía el mensaje por serial
}
