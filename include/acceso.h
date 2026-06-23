#ifndef ACCESO_H
#define ACCESO_H
#include <avr/io.h>

// Tipos de acceso
#define ACCESO_DENEGADO  0
#define ACCESO_ADULTO    1
#define ACCESO_HIJO      2

void acceso_init();
void acceso_abrir_principal();  // activa relé
void acceso_cerrar_principal(); // desactiva relé
void acceso_abrir_garaje();     // motor PAP horario
void acceso_cerrar_garaje();    // motor PAP antihorario

#endif
