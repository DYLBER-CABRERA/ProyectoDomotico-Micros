#include <avr/io.h>
#include <avr/eeprom.h>
#include <string.h>
#include "../include/eeprom_mgr.h"

// Tamanho de cada slot de UID en EEPROM: 4 bytes de UID + 1 byte de tipo
#define TAM_SLOT_UID  5

// Marca de slot vacio: 0xFF (EEPROM virgen o borrada)
#define SLOT_VACIO    0xFF

// Tipo de acceso
#define TIPO_ADULTO   0
#define TIPO_HIJO     1

// -- Funcion interna: direccion base del slot i ---------------------------
static uint16_t direccion_slot(uint8_t indice) {
    return EEPROM_BASE_UIDS + (indice * TAM_SLOT_UID);
}

// -- eeprom_init() -----------------------------------------------------------
void eeprom_init() {
    // Sin configuracion de hardware necesaria; la EEPROM esta lista al arranque.
    // La funcion existe por consistencia con el patron del proyecto.
}

// -- eeprom_escribir(dir, dato) ------------------------------------------------
void eeprom_escribir(uint16_t dir, uint8_t dato) {
    // Proteccion de ciclos de escritura: solo escribir si el valor va a cambiar
    if (eeprom_read_byte((const uint8_t*)dir) != dato) {
        eeprom_write_byte((uint8_t*)dir, dato);
    }
}

// -- eeprom_leer(dir) -----------------------------------------------------------
uint8_t eeprom_leer(uint16_t dir) {
    return eeprom_read_byte((const uint8_t*)dir);
}

// -- eeprom_leer_tiempo(indice) ------------------------------------------------
int16_t eeprom_leer_tiempo(uint8_t indice) {
    if (indice >= EEPROM_MAX_UIDS) return 0;
    uint16_t dir = HIJO_TIEMPO_BASE + (indice * HIJO_TIEMPO_TAM);
    int16_t t;
    uint8_t* p = (uint8_t*)&t;
    p[0] = eeprom_read_byte((const uint8_t*)dir);
    p[1] = eeprom_read_byte((const uint8_t*)(dir + 1));
    return t;
}

// -- eeprom_escribir_tiempo(indice, tiempo) ------------------------------------
void eeprom_escribir_tiempo(uint8_t indice, int16_t tiempo) {
    if (indice >= EEPROM_MAX_UIDS) return;
    uint16_t dir = HIJO_TIEMPO_BASE + (indice * HIJO_TIEMPO_TAM);
    uint8_t* p = (uint8_t*)&tiempo;
    if (eeprom_read_byte((const uint8_t*)dir) != p[0])
        eeprom_write_byte((uint8_t*)dir, p[0]);
    if (eeprom_read_byte((const uint8_t*)(dir + 1)) != p[1])
        eeprom_write_byte((uint8_t*)(dir + 1), p[1]);
}

// -- eeprom_leer_estado(indice) ------------------------------------------------
uint8_t eeprom_leer_estado(uint8_t indice) {
    if (indice >= EEPROM_MAX_UIDS) return HIJO_FUERA;
    uint16_t dir = HIJO_TIEMPO_BASE + (indice * HIJO_TIEMPO_TAM) + 2;
    return eeprom_read_byte((const uint8_t*)dir);
}

// -- eeprom_escribir_estado(indice, estado) ------------------------------------
void eeprom_escribir_estado(uint8_t indice, uint8_t estado) {
    if (indice >= EEPROM_MAX_UIDS) return;
    uint16_t dir = HIJO_TIEMPO_BASE + (indice * HIJO_TIEMPO_TAM) + 2;
    if (eeprom_read_byte((const uint8_t*)dir) != estado)
        eeprom_write_byte((uint8_t*)dir, estado);
}

// -- eeprom_buscar_uid(uid) -----------------------------------------------------
// Busca un UID en los slots. Si lo encuentra, retorna el indice (1-10).
// Si no existe, retorna 0xFF.
uint8_t eeprom_buscar_uid(uint8_t* uid) {
    for (uint8_t i = 0; i < EEPROM_MAX_UIDS; i++) {
        uint16_t dir = direccion_slot(i);

        // Slot vacio -> saltar
        if (eeprom_read_byte((const uint8_t*)dir) == SLOT_VACIO) continue;

        // Comparar los 4 bytes del UID
        uint8_t coincide = 1;
        for (uint8_t b = 0; b < 4; b++) {
            if (eeprom_read_byte((const uint8_t*)(dir + b)) != uid[b]) {
                coincide = 0;
                break;
            }
        }

        if (coincide) return i;  // encontrado
    }

    return 0xFF;  // no existe
}

// -- eeprom_guardar_uid(uid, tipo) ------------------------------------------------
// Guarda un UID en el primer slot libre. Retorna el indice (1-10) o 0xFF si llena.
// tipo: 0 = adulto, 1 = hijo
uint8_t eeprom_guardar_uid(uint8_t* uid, uint8_t tipo) {
    // Verificar que no exista ya
    if (eeprom_buscar_uid(uid) != 0xFF) return 0xFF;

    // Buscar primer slot libre
    for (uint8_t i = 0; i < EEPROM_MAX_UIDS; i++) {
        uint16_t dir = direccion_slot(i);

        if (eeprom_read_byte((const uint8_t*)dir) == SLOT_VACIO) {
            // Escribir los 4 bytes del UID
            for (uint8_t b = 0; b < 4; b++) {
                eeprom_escribir(dir + b, uid[b]);
            }

            // Escribir el tipo en el 5o byte
            eeprom_escribir(dir + 4, tipo);

            return i;  // guardado exitosamente
        }
    }

    return 0xFF;  // no hay espacio
}

// -- eeprom_borrar_uid(uid) -----------------------------------------------------
// Busca el UID y marca su slot como vacio. Retorna 1 si lo encontro, 0 si no.
uint8_t eeprom_borrar_uid(uint8_t* uid) {
    uint8_t indice = eeprom_buscar_uid(uid);
    if (indice == 0xFF) return 0;  // no existe

    uint16_t dir = direccion_slot(indice);

    // Marcar el primer byte como SLOT_VACIO (0xFF) para liberar el slot
    // No es necesario borrar todo el registro
    eeprom_escribir(dir, SLOT_VACIO);

    return 1;
}
