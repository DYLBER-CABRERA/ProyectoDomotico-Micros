// mercado.cpp - Lista de mercado persistente en EEPROM
// Sin librerias - registros directos ATmega2560 (avr/eeprom.h permitida)

#include <avr/io.h>
#include <avr/eeprom.h>
#include <string.h>
#include "../include/mercado.h"


// -- Funcion interna: calcular direccion de un slot ---------------------
// static -> privada de este archivo
static uint16_t direccion_slot(uint8_t indice) {
    return MERCADO_BASE_EEPROM + (indice * MERCADO_TAM_REGISTRO);
}


// -- Funcion interna: saber si un slot esta LIBRE -----------------------
// Una EEPROM en blanco (nunca escrita) vale 0xFF, no 0x00. Antes el codigo
// solo consideraba libre el slot con primer byte 0x00, por lo que en una
// EEPROM virgen NINGUN slot parecia libre y el mercado salia "lleno" siempre.
// Aqui se considera libre tanto 0x00 (borrado) como 0xFF (nunca escrito).
static uint8_t slot_libre(uint8_t primer_byte) {
    return (primer_byte == 0x00 || primer_byte == 0xFF);
}


// -- mercado_init() -----------------------------------------------------------
void mercado_init() {
    // La EEPROM ya esta lista desde el arranque -- sin configuracion
    // de hardware necesaria. Funcion presente por consistencia con
    // el patron del resto de modulos del proyecto.
}


// -- mercado_agregar(nombre, cantidad) -----------------------------------------------------------
uint8_t mercado_agregar(const char* nombre, uint8_t cantidad) {

    char nombre_leido[MERCADO_NOMBRE_LEN + 1];

    // Primero verificar que el producto no exista ya (evita duplicados
    // y escrituras innecesarias en EEPROM -- proteccion de ciclos)
    for (uint8_t i = 0; i < MERCADO_MAX_ITEMS; i++) {

        uint16_t dir = direccion_slot(i);

        // Leer el nombre guardado en este slot, byte a byte
        for (uint8_t b = 0; b <= MERCADO_NOMBRE_LEN; b++) {
            nombre_leido[b] = (char)eeprom_read_byte((const uint8_t*)(dir + b));
        }

        // Si coincide exactamente con el nombre que se quiere agregar,
        // no se duplica -- se considera que ya existe
        if (strncmp(nombre_leido, nombre, MERCADO_NOMBRE_LEN) == 0) {
            return 0; // ya existe, no se agrega de nuevo
        }
    }

    // Buscar el primer slot vacio (nombre_leido[0] == MERCADO_SLOT_VACIO)
    for (uint8_t i = 0; i < MERCADO_MAX_ITEMS; i++) {

        uint16_t dir = direccion_slot(i);

        uint8_t primer_byte = eeprom_read_byte((const uint8_t*)dir);

        if (slot_libre(primer_byte)) {

            // Escribir el nombre, caracter por caracter, hasta
            // MERCADO_NOMBRE_LEN caracteres o hasta el '\0' del string
            uint8_t b;
            for (b = 0; b < MERCADO_NOMBRE_LEN && nombre[b] != '\0'; b++) {
                eeprom_write_byte((uint8_t*)(dir + b), (uint8_t)nombre[b]);
            }

            // Completar con '\0' el resto del campo nombre (incluye
            // la posicion del terminador aunque el nombre llene todo
            // el espacio disponible)
            for (; b <= MERCADO_NOMBRE_LEN; b++) {
                eeprom_write_byte((uint8_t*)(dir + b), 0x00);
            }

            // Escribir la cantidad en el ultimo byte del registro
            eeprom_write_byte((uint8_t*)(dir + MERCADO_NOMBRE_LEN + 1), cantidad);

            return 1; // agregado exitosamente
        }
    }

    return 0; // no hay espacio -- los MERCADO_MAX_ITEMS slots estan ocupados
}


// -- mercado_eliminar(nombre) -----------------------------------------------------------
uint8_t mercado_eliminar(const char* nombre) {

    char nombre_leido[MERCADO_NOMBRE_LEN + 1];

    for (uint8_t i = 0; i < MERCADO_MAX_ITEMS; i++) {

        uint16_t dir = direccion_slot(i);

        for (uint8_t b = 0; b <= MERCADO_NOMBRE_LEN; b++) {
            nombre_leido[b] = (char)eeprom_read_byte((const uint8_t*)(dir + b));
        }

        if (strncmp(nombre_leido, nombre, MERCADO_NOMBRE_LEN) == 0) {

            // PROTECCION DE CICLOS: solo escribir si el slot no esta
            // ya vacio (evita un ciclo de escritura redundante)
            uint8_t primer_byte = eeprom_read_byte((const uint8_t*)dir);

            if (!slot_libre(primer_byte)) {
                // Marcar el slot como vacio escribiendo 0x00 en el
                // primer byte (suficiente para considerarlo libre,
                // no es necesario borrar todo el registro)
                eeprom_write_byte((uint8_t*)dir, MERCADO_SLOT_VACIO);
            }

            return 1; // encontrado y eliminado
        }
    }

    return 0; // no se encontro ese producto
}


// -- mercado_listar() -----------------------------------------------------------
// Declaracion adelantada de las funciones de usart usadas aqui.
// Se evita incluir usart.h directamente para no crear dependencia
// circular entre modulos -- el .ino las declara externamente.
extern void usart_enviar_string(const char* s);
extern void usart_enviar_int(int16_t n);
extern void usart_enviar_newline();

void mercado_listar() {

    char nombre_leido[MERCADO_NOMBRE_LEN + 1];
    uint8_t encontrados = 0;

    for (uint8_t i = 0; i < MERCADO_MAX_ITEMS; i++) {

        uint16_t dir = direccion_slot(i);

        uint8_t primer_byte = eeprom_read_byte((const uint8_t*)dir);

        if (!slot_libre(primer_byte)) {

            // Leer el nombre completo
            for (uint8_t b = 0; b <= MERCADO_NOMBRE_LEN; b++) {
                nombre_leido[b] = (char)eeprom_read_byte((const uint8_t*)(dir + b));
            }

            // Leer la cantidad
            uint8_t cantidad = eeprom_read_byte((const uint8_t*)(dir + MERCADO_NOMBRE_LEN + 1));

            // Enviar en formato "nombre x cantidad"
            usart_enviar_string(nombre_leido);
            usart_enviar_string(" x ");
            usart_enviar_int(cantidad);
            usart_enviar_newline();

            encontrados++;
        }
    }

    if (encontrados == 0) {
        usart_enviar_string("LISTA VACIA");
        usart_enviar_newline();
    }
}


// -- mercado_contar() -----------------------------------------------------------
uint8_t mercado_contar() {

    uint8_t total = 0;

    for (uint8_t i = 0; i < MERCADO_MAX_ITEMS; i++) {

        uint16_t dir = direccion_slot(i);
        uint8_t primer_byte = eeprom_read_byte((const uint8_t*)dir);

        if (!slot_libre(primer_byte)) {
            total++;
        }
    }

    return total;
}


// -- mercado_leer_indice(indice, nombre, cantidad) ---------------------------
// Lee un slot por indice para mostrarlo en el LCD (sin depender del serial).
uint8_t mercado_leer_indice(uint8_t indice, char* nombre, uint8_t* cantidad) {

    if (indice >= MERCADO_MAX_ITEMS) return 0;

    uint16_t dir = direccion_slot(indice);

    // Slot vacio: nada que leer (0x00 borrado o 0xFF nunca escrito)
    if (slot_libre(eeprom_read_byte((const uint8_t*)dir))) return 0;

    // Copiar el nombre completo (incluye el '\0' del campo)
    for (uint8_t b = 0; b <= MERCADO_NOMBRE_LEN; b++) {
        nombre[b] = (char)eeprom_read_byte((const uint8_t*)(dir + b));
    }
    nombre[MERCADO_NOMBRE_LEN] = '\0'; // garantizar terminador

    // Leer la cantidad (ultimo byte del registro)
    *cantidad = eeprom_read_byte((const uint8_t*)(dir + MERCADO_NOMBRE_LEN + 1));

    return 1;
}
