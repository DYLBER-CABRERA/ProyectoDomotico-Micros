// =============================================================================
// historial.cpp  —  Sustentacion Microprocesadores Historial
// =============================================================================
// Implementacion del historial de accesos RFID usando un ring buffer en la
// EEPROM interna del ATmega2560.
//
// ESTRUCTURA EN EEPROM (zona libre a partir de 0x200):
//
//   Direccion  Contenido
//   ─────────────────────────────────────────────────────────────────────────
//   0x200      head  (1 byte): indice del slot mas antiguo del ring buffer.
//                              Cuando el buffer esta lleno y se inserta una
//                              nueva entrada, head avanza circularmente para
//                              apuntar al nuevo slot mas antiguo.
//   0x201      count (1 byte): numero de entradas validas (0-20).
//                              Si count < 20 el buffer tiene espacio libre;
//                              si count == 20 el buffer esta lleno y head
//                              indica donde sobreescribir la mas antigua.
//   0x202-0x279 datos: 20 entradas x 6 bytes cada una.
//                              byte 0: tipo  (0=ADULTO / 1=HIJO)
//                              byte 1: lugar (0=PUERTA / 1=GARAJE / 2=SALA)
//                              bytes 2-5: UID[4] de la tarjeta RFID
//   ─────────────────────────────────────────────────────────────────────────
//   Total: 2 bytes metadata + 120 bytes datos = 122 bytes ocupados.
//
// POLITICA DE ESCRITURA EEPROM: antes de cada eeprom_write_byte se comprueba
// si el byte ya tiene el valor deseado. Si coincide, NO se escribe, evitando
// gastar ciclos de escritura innecesariamente (la EEPROM soporta ~100.000
// ciclos por direccion; con esta proteccion la durabilidad es mucho mayor).
//
// RING BUFFER FIFO CIRCULAR:
//   - Mientras count < 20: se escribe en el indice 'count' y count++.
//   - Cuando count == 20 (lleno): se sobreescribe en el indice 'head' y
//     head avanza al siguiente (head = (head+1) % 20). Asi siempre se
//     conservan los 20 accesos mas recientes sin desperdiciar mas EEPROM.
//   - Al listar, se recorre desde head hasta head+count-1 (modulo 20),
//     lo que entrega las entradas de mas antigua a mas reciente.
// =============================================================================

#include <avr/io.h>
#include <avr/eeprom.h>
#include "../include/historial.h"

// Declaraciones externas de USART (evita dependencia circular con usart.h)
extern void usart_enviar_string(const char* s);
extern void usart_enviar_newline();

// Mapa EEPROM del historial (zona libre a partir de 0x200):
//   0x200 : head  — indice del slot mas antiguo (ring buffer)
//   0x201 : count — numero de entradas almacenadas (0..HIST_MAX)
//   0x202 – 0x279 : datos, 20 entradas x 6 bytes
//
// Layout de cada entrada (6 bytes):
//   byte 0 : tipo  (HIST_TIPO_ADULTO=0 / HIST_TIPO_HIJO=1)
//   byte 1 : lugar (0=PUERTA, 1=GARAJE, 2=SALA)
//   bytes 2-5 : UID[4]
#define HIST_BASE_DATOS  0x202u   // primera entrada del ring buffer
#define HIST_MAX         20u      // maximo de entradas almacenadas
#define HIST_ENTRY_SIZE  6u       // bytes por entrada (tipo+lugar+uid[4])

// ─────────────────────────────────────────────────────────────────────────────
// h_leer / h_escribir
// Wrappers internos de lectura/escritura EEPROM.
// h_escribir compara antes de escribir para no gastar ciclos de escritura si
// el byte ya tiene el valor correcto (proteccion de durabilidad).
// Se usan en vez de eeprom_leer/escribir de eeprom_mgr para no crear una
// dependencia de modulo innecesaria: historial es independiente de eeprom_mgr.
// ─────────────────────────────────────────────────────────────────────────────
static uint8_t h_leer(uint16_t dir) {
    return eeprom_read_byte((const uint8_t*)dir);
}

static void h_escribir(uint16_t dir, uint8_t dato) {
    // Solo escribe si el valor cambia: protege los ciclos de escritura de la EEPROM
    if (eeprom_read_byte((const uint8_t*)dir) != dato) {
        eeprom_write_byte((uint8_t*)dir, dato);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// historial_init()
// Sustentacion Microprocesadores Historial — inicializacion del modulo.
//
// Se llama desde setup() despues de eeprom_init() y antes de rfid_init().
// Verifica si los punteros del ring buffer en EEPROM (head y count) son
// validos. Si la EEPROM esta virgen (valor 0xFF al salir de fabrica) o si
// count excede HIST_MAX (dato corrupto), resetea ambos punteros a cero.
// Esto garantiza que historial_registrar y historial_listar siempre partan
// de un estado consistente, incluso en la primera ejecucion del firmware.
// No borra las entradas de datos: solo los punteros de control.
// ─────────────────────────────────────────────────────────────────────────────
void historial_init() {
    uint8_t count = h_leer(0x201);
    // 0xFF = EEPROM virgen; >HIST_MAX = dato corrupto. En ambos casos resetear.
    if (count == 0xFF || count > HIST_MAX) {
        h_escribir(0x200, 0);  // head  = 0 (empezar desde el primer slot)
        h_escribir(0x201, 0);  // count = 0 (ningun registro valido aun)
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// historial_registrar(tipo, lugar, uid)
// Sustentacion Microprocesadores Historial — registro de un acceso concedido.
//
// Inserta una nueva entrada en el ring buffer. Se llama desde rfid.cpp
// inmediatamente despues de que el sistema decide CONCEDER el acceso, en
// cualquiera de los 4 puntos de acceso exitoso:
//   - SALA + adulto: tipo=ADULTO, lugar=SALA
//   - SALA + hijo (descuento exitoso): tipo=HIJO, lugar=SALA
//   - GARAJE (cualquier tarjeta autorizada): tipo segun EEPROM, lugar=GARAJE
//   - PUERTA (cualquier tarjeta autorizada): tipo segun EEPROM, lugar=PUERTA
//
// PARAMETROS:
//   tipo  — HIST_TIPO_ADULTO (0) o HIST_TIPO_HIJO (1)
//   lugar — 0=PUERTA / 1=GARAJE / 2=SALA (coincide con LUGAR_x de acceso.h)
//   uid   — puntero a los 4 bytes del UID leido por rfid_leer_uid()
//
// ALGORITMO DEL RING BUFFER:
//   Si hay espacio (count < 20): escribe en el indice 'count' y sube count.
//   Si esta lleno (count == 20): sobreescribe en 'head' y avanza head.
//   El costo es siempre 8 escrituras EEPROM maximo (~24 ms en total) porque
//   h_escribir solo escribe si el valor cambia realmente.
// ─────────────────────────────────────────────────────────────────────────────
void historial_registrar(uint8_t tipo, uint8_t lugar, uint8_t* uid) {
    uint8_t head  = h_leer(0x200);
    uint8_t count = h_leer(0x201);

    uint8_t idx;
    if (count < HIST_MAX) {
        // Hay espacio libre: escribir al final logico del buffer
        idx = count;
        count++;
    } else {
        // Buffer lleno: sobreescribir la entrada mas antigua y avanzar head.
        // head apunta al slot mas antiguo; tras avanzar, el nuevo head sera
        // el siguiente mas antiguo, preservando el orden cronologico.
        idx  = head;
        head = (uint8_t)((head + 1u) % HIST_MAX);
    }

    // Calcular la direccion base del slot elegido y escribir los 6 bytes.
    uint16_t dir = HIST_BASE_DATOS + ((uint16_t)idx * HIST_ENTRY_SIZE);
    h_escribir(dir + 0u, tipo);   // byte 0: quien ingreso (adulto/hijo)
    h_escribir(dir + 1u, lugar);  // byte 1: por donde ingreso
    for (uint8_t i = 0; i < 4u; i++) {
        h_escribir(dir + 2u + i, uid[i]);  // bytes 2-5: UID de la tarjeta
    }

    // Actualizar los punteros de control en EEPROM
    h_escribir(0x200, head);
    h_escribir(0x201, count);
}

// ─────────────────────────────────────────────────────────────────────────────
// enviar_hex(b)
// Sustentacion Microprocesadores Historial — helper interno de formateo.
//
// Convierte un byte a su representacion hexadecimal de 2 digitos mayusculos
// y lo envia por USART. Se usa en historial_listar() para mostrar el UID
// de cada tarjeta en el formato AB:CD:EF:12, igual que los lectores RFID
// convencionales. No usa sprintf (ahorra ~1.5 KB de Flash en AVR).
// ─────────────────────────────────────────────────────────────────────────────
static void enviar_hex(uint8_t b) {
    const char* HEX = "0123456789AB";
    char buf[3];
    buf[0] = HEX[(b >> 4) & 0x0Fu];  // nibble alto -> primer digito
    buf[1] = HEX[b & 0x0Fu];          // nibble bajo -> segundo digito
    buf[2] = '\0';
    usart_enviar_string(buf);
}

// ─────────────────────────────────────────────────────────────────────────────
// historial_listar()
// Sustentacion Microprocesadores Historial — consulta del historial por serial.
//
// Recorre el ring buffer de la entrada mas antigua a la mas reciente y envia
// cada registro por USART0 (9600 8N1). Se activa con el comando serial
// "HISTORIAL" o con la combinacion de teclado *71#.
//
// FORMATO DE SALIDA por cada entrada:
//   ADULTO,PUERTA,AB:CD:EF:12
//   HIJO,SALA,11:22:33:44
//
// ORDEN DE RECORRIDO: comenzando en head y avanzando 'count' pasos modulo
// HIST_MAX, se visitan los slots en orden cronologico (mas antiguo primero).
// Este orden es el mismo independientemente de cuantas veces haya dado la
// vuelta el ring buffer.
// ─────────────────────────────────────────────────────────────────────────────
void historial_listar() {
    uint8_t head  = h_leer(0x200);
    uint8_t count = h_leer(0x201);

    if (count == 0) {
        // No hay entradas: informar y salir sin recorrer nada
        usart_enviar_string("(sin registros)");
        usart_enviar_newline();
        return;
    }

    for (uint8_t i = 0; i < count; i++) {
        // idx: indice del slot i-esimo en orden cronologico
        uint8_t  idx = (uint8_t)((head + i) % HIST_MAX);
        uint16_t dir = HIST_BASE_DATOS + ((uint16_t)idx * HIST_ENTRY_SIZE);

        uint8_t tipo  = h_leer(dir + 0u);
        uint8_t lugar = h_leer(dir + 1u);

        // Columna 1: tipo de usuario
        if (tipo == HIST_TIPO_ADULTO) usart_enviar_string("ADULTO");
        else                          usart_enviar_string("HIJO");

        usart_enviar_string(",");

        // Columna 2: lugar de acceso
        if      (lugar == 0u) usart_enviar_string("PUERTA");
        else if (lugar == 1u) usart_enviar_string("GARAJE");
        else                  usart_enviar_string("SALA");

        usart_enviar_string(",");

        // Columna 3: UID de 4 bytes en hex separados por ':' (ej. AB:CD:EF:12)
        for (uint8_t b = 0; b < 4u; b++) {
            if (b > 0) usart_enviar_string(":");
            enviar_hex(h_leer(dir + 2u + b));
        }

        usart_enviar_newline();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// historial_limpiar()
// Sustentacion Microprocesadores Historial — borrado del historial.
//
// Resetea head y count a cero. Las entradas de datos antiguas quedan fisicamente
// en la EEPROM pero jamas se leeran (count=0 impide el recorrido). Esto evita
// N escrituras EEPROM innecesarias de borrado byte a byte: es suficiente con
// invalidar los punteros de control.
// Se activa con el comando serial "HISTORIAL:LIMPIAR" o con *70# por teclado.
// ─────────────────────────────────────────────────────────────────────────────
void historial_limpiar() {
    h_escribir(0x200, 0);  // head  = 0 (siguiente escritura empieza en slot 0)
    h_escribir(0x201, 0);  // count = 0 (historial vacio: listar no recorre nada)
}
