#ifndef ACCESO_H
#define ACCESO_H
#include <avr/io.h>

// Tipos de acceso
#define ACCESO_DENEGADO  0
#define ACCESO_ADULTO    1
#define ACCESO_HIJO      2

// Lugar de acceso seleccionado con los 2 pulsadores (lista circular).
// Lo usan el .ino (selector + retardo) y rfid.cpp (decide la accion).
#define LUGAR_PUERTA  0   // puerta principal -> conceder + 10s para desarmar
#define LUGAR_GARAJE  1   // garaje           -> conceder + 15s para desarmar
#define LUGAR_SALA    2   // sala de juegos   -> contador de cupos del nino
#define LUGAR_TOTAL   3   // cantidad de lugares (para el ciclo circular)

// Pin del iman/relé de la puerta principal (PG0 / D41)
#define IMAN_DDR    DDRG
#define IMAN_PORT   PORTG
#define IMAN_PIN    PG0

void acceso_init();
void acceso_abrir_principal();  // activa relé (iman)
void acceso_cerrar_principal(); // desactiva relé
void acceso_abrir_garaje();     // servo horario
void acceso_cerrar_garaje();    // servo antihorario
void acceso_concedido_adulto(); // abre puerta principal
void acceso_concedido_hijo();   // solo actualiza LCD/serial (sala juegos)
void acceso_denegado();         // feedback de denegacion

#endif
