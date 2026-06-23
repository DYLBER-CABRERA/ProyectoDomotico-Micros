// usart.cpp - Driver USART0 ATmega2560
// Sin librerias - registros directos
// 9600 bps, 8N1, ISR de recepcion con buffer circular

#include <avr/io.h>
#include <avr/interrupt.h>
#include "../include/usart.h"


// -- Buffer circular de recepcion ------------------------------------
// Un buffer circular (ring buffer) permite que la ISR deposite bytes
// mientras el loop principal los consume, sin perdidas y sin bloqueos.
//
// Funciona con dos indices:
//   cabeza (head) -> donde la ISR escribe el proximo byte
//   cola  (tail) -> donde el loop lee el proximo byte
//
// Buffer vacio : head == tail
// Buffer lleno : (head + 1) & MASK == tail  (se deja 1 espacio libre)
//
// La mascara & USART_RX_BUF_MASK reemplaza el modulo (%) y es mas
// rapida porque USART_RX_BUF_SIZE es potencia de 2.

static volatile uint8_t rx_buf[USART_RX_BUF_SIZE]; // datos recibidos
static volatile uint8_t rx_head = 0;  // indice de escritura (ISR)
static volatile uint8_t rx_tail = 0;  // indice de lectura  (loop)


// -- ISR de recepcion ------------------------------------------------
// Se dispara automaticamente cuando llega un byte completo por RX0.
// REGLA DEL PROYECTO: ISR minima -> solo depositar en buffer.
// NO llama a lcd_string(), NO procesa el comando aqui.
// En usart.cpp, dentro de ISR(USART0_RX_vect):
ISR(USART0_RX_vect) {

    uint8_t byte_recibido = UDR0;

    // NUEVO: eco inmediato del caracter recibido
    // Espera que UDR0 este libre antes de escribir el eco
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = byte_recibido;

    uint8_t next_head = (rx_head + 1) & USART_RX_BUF_MASK;

    if (next_head != rx_tail) {
        rx_buf[rx_head] = byte_recibido;
        rx_head = next_head;
    }
}


// -- usart_init() ----------------------------------------------------
// Configura USART0 a 9600 bps 8N1 con ISR de RX habilitada.
// Llamar en setup() ANTES de sei().
void usart_init() {

    // UBRR0H y UBRR0L: registros de baud rate (16 bits divididos en 2)
    // Se escribe primero el byte alto (H) y luego el bajo (L).
    // USART_UBRR = 103 = 0x0067 -> H=0x00, L=0x67
    //
    // REGLA ATmega2560: en registros de 16 bits siempre escribir
    // el byte alto primero, luego el bajo.
    UBRR0H = (uint8_t)(USART_UBRR >> 8);  // byte alto del baud rate
    UBRR0L = (uint8_t)(USART_UBRR);       // byte bajo del baud rate

    // UCSR0A: registro de estado y control A del USART0
    // U2X0 = 0 -> modo normal (no doble velocidad)
    // Limpiar el registro para asegurar estado conocido
    UCSR0A = 0x00;

    // UCSR0B: registro de control B del USART0
    // (1 << RXCIE0) -> habilita la interrupcion de recepcion completa
    //                  Cuando llega un byte, dispara USART0_RX_vect
    // (1 << RXEN0)  -> habilita el RECEPTOR (RX)
    // (1 << TXEN0)  -> habilita el TRANSMISOR (TX)
    //
    // Sin RXEN0 y TXEN0 el hardware no conecta los pines PE0/PE1
    // al USART y no recibe ni transmite nada.
    UCSR0B = (1 << RXCIE0) | (1 << RXEN0) | (1 << TXEN0);

    // UCSR0C: registro de control C del USART0
    // (1 << UCSZ01) | (1 << UCSZ00) -> 8 bits de datos (UCSZ=0b11)
    // UPM01=0, UPM00=0                -> sin paridad
    // USBS0=0                         -> 1 bit de stop
    // Resultado: formato 8N1 (el mas universal)
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);

    // Limpiar el buffer de recepcion
    rx_head = 0;
    rx_tail = 0;
}


// -- usart_enviar_char(c) --------------------------------------------
// Envia un byte por TX0. Bloqueante: espera que el registro
// de transmision este libre antes de escribir.
// Para TX el bloqueo es aceptable porque transmitir es rapido
// (~1ms por caracter a 9600 bps) y no perdemos datos criticos.
void usart_enviar_char(char c) {

    // UCSR0A bit UDRE0: "USART Data Register Empty"
    // UDRE0=1 significa que UDR0 esta vacio y listo para recibir dato
    // UDRE0=0 significa que el hardware aun esta transmitiendo el anterior
    //
    // Esperar activamente hasta que el registro este libre
    // loop_until_bit_is_set() es una macro de avr-libc que hace esto:
    //   while (!(UCSR0A & (1 << UDRE0)));
    while (!(UCSR0A & (1 << UDRE0)));

    // Escribir el byte en UDR0 -> el hardware inicia la transmision
    // automaticamente en cuanto se escribe aqui
    UDR0 = (uint8_t)c;
}


// -- usart_enviar_string(s) ------------------------------------------
// Envia una cadena de texto completa byte a byte.
void usart_enviar_string(const char* s) {

    // Recorrer el string hasta el terminador '\0'
    while (*s) {
        usart_enviar_char(*s++); // enviar caracter y avanzar puntero
    }
}


// -- usart_enviar_int(n) ---------------------------------------------
// Convierte un entero a texto y lo envia por serial.
// Ejemplo: n=123 -> envia los caracteres '1','2','3'
void usart_enviar_int(int16_t n) {

    char buf[7];   // buffer: maximo 5 digitos + signo + '\0'
    int8_t i = 0;

    // Manejar numero negativo
    if (n < 0) {
        usart_enviar_char('-');
        n = -n;
    }

    // Caso especial cero
    if (n == 0) {
        usart_enviar_char('0');
        return;
    }

    // Extraer digitos al reves (menos significativo primero)
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }

    // Enviar en orden correcto (de atras hacia adelante)
    while (i > 0) {
        usart_enviar_char(buf[--i]);
    }
}


// -- usart_enviar_newline() ------------------------------------------
// Envia \r\n (CRLF). Necesario para que la Virtual Terminal de Proteus
// y los monitores seriales de Windows muestren el salto de linea.
void usart_enviar_newline() {
    usart_enviar_char('\r');  // Carriage Return
    usart_enviar_char('\n');  // Line Feed
}


// -- usart_hay_dato() ------------------------------------------------
// Retorna 1 si hay al menos un byte en el buffer de recepcion.
// No consume el dato - solo verifica.
uint8_t usart_hay_dato() {
    // Si head != tail hay datos en el buffer
    // head == tail significa buffer vacio
    return (rx_head != rx_tail) ? 1 : 0;
}


// -- usart_recibir_char() --------------------------------------------
// Lee y consume el siguiente byte del buffer circular.
// Retorna '\0' si el buffer esta vacio.
char usart_recibir_char() {

    // Verificar que hay algo que leer
    if (rx_head == rx_tail) return '\0';

    // Leer el byte en la posicion actual de cola (tail)
    char c = (char)rx_buf[rx_tail];

    // Avanzar el indice de cola con wrap-around circular
    rx_tail = (rx_tail + 1) & USART_RX_BUF_MASK;

    return c;
}


// -- usart_hay_linea() -----------------------------------------------
// Retorna 1 si hay un '\n' en el buffer (linea completa recibida).
// Permite al parser de comandos saber cuando hay un comando completo.
uint8_t usart_hay_linea() {

    uint8_t i = rx_tail; // empezar desde la posicion de lectura

    // Recorrer el buffer hasta llegar a head (todos los datos actuales)
    while (i != rx_head) {
        // acepta cualquiera de los dos como fin de linea
        if (rx_buf[i] == '\n' || rx_buf[i] == '\r') return 1; // encontro fin de linea
        i = (i + 1) & USART_RX_BUF_MASK; // avanzar circularmente
    }
    return 0; // no hay '\n' aun
}


// -- usart_leer_linea(dest) ------------------------------------------
// Copia los bytes del buffer hasta '\n' o '\r' a dest[],
// descarta el companero sobrante ('\n' tras '\r' o viceversa),
// procesa backspace (0x08 / 0x7F) para permitir edicion en la terminal,
// y agrega '\0' al final. Retorna la cantidad de caracteres utiles.
// dest[] debe tener al menos USART_RX_BUF_SIZE bytes.
uint8_t usart_leer_linea(char* dest) {

    uint8_t n = 0;
    char c;

    while (usart_hay_dato() && n < (USART_RX_BUF_SIZE - 1)) {

        c = usart_recibir_char();

        if (c == '\n' || c == '\r') {
            // Descartar el companero sobrante del par \r\n o \n\r
            // para que no genere una linea vacia en el siguiente ciclo.
            if (usart_hay_dato()) {
                char siguiente = (char)rx_buf[rx_tail];
                if ((c == '\r' && siguiente == '\n') ||
                    (c == '\n' && siguiente == '\r')) {
                    rx_tail = (rx_tail + 1) & USART_RX_BUF_MASK;
                }
            }
            break;
        }

        if (c == '\b' || c == 0x7F) {
            // Backspace: eliminar el ultimo caracter acumulado
            if (n > 0) n--;
        } else {
            dest[n++] = c;
        }
    }

    dest[n] = '\0';
    return n;
}

