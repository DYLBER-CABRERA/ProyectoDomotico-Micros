// lcd.cpp — implementación pendiente (Fase 1)
// Adaptar del taller HOLA MUNDO anterior
// ============================================================
// lcd.cpp — Driver LCD 16x2 HD44780 en modo 4 bits
// Puerto A del ATmega2560
// ============================================================

// avr/io.h   → define los nombres de registros y puertos
//              (DDRA, PORTA, PA0, PA2, etc.)
#include <avr/io.h>

// util/delay.h → permite usar _delay_ms() y _delay_us()
//                son delays precisos basados en F_CPU
#include <util/delay.h>

// lcd.h ? incluye las definiciones de pines y los prototipos
//          de las funciones p�blicas de este m�dulo
#include "../include/lcd.h"


// ============================================================
// FUNCIONES INTERNAS (privadas)
// La palabra "static" hace que solo sean visibles dentro de
// este archivo .cpp — ningún otro módulo puede llamarlas
// ============================================================

// ------------------------------------------------------------
// pulso_en()
// Genera el pulso en el pin EN que le indica al LCD:
// "ahora lee lo que hay en los pines de datos"
// El HD44780 lee en el flanco de BAJADA del pin EN
// ------------------------------------------------------------
static void pulso_en() {

    // LCD_PORT |= (1 << LCD_EN)
    // |=  → operación OR bit a bit para poner solo ese bit en 1
    //        sin modificar los demás bits del puerto
    // (1 << LCD_EN) → desplaza el valor 1 hasta la posición
    //                  del bit LCD_EN (PA2 = bit 2)
    // Resultado: EN sube a 1 (flanco de subida)
    LCD_PORT |=  (1 << LCD_EN);

    // Esperar 1 microsegundo — el HD44780 necesita que EN
    // permanezca en 1 al menos 450ns antes de bajar
    _delay_us(1);

    // LCD_PORT &= ~(1 << LCD_EN)
    // ~(1 << LCD_EN) → invierte todos los bits: el bit LCD_EN
    //                   queda en 0 y todos los demás en 1
    // &=  → AND bit a bit: solo baja el bit LCD_EN, el resto
    //        no cambia
    // Resultado: EN baja a 0 (flanco de bajada — aquí el LCD
    //            captura el dato)
    LCD_PORT &= ~(1 << LCD_EN);

    // Esperar 50 microsegundos para que el LCD procese el dato
    // El HD44780 necesita mínimo 37µs para ejecutar un comando
    _delay_us(50);
}


// ------------------------------------------------------------
// enviar_nibble(nibble)
// Pone 4 bits en PA4–PA7 y genera el pulso EN
// En modo 4 bits los datos viajan por los 4 pines altos
// del puerto (PA4=D4, PA5=D5, PA6=D6, PA7=D7)
// ------------------------------------------------------------
static void enviar_nibble(uint8_t nibble) {

    // LCD_PORT &= 0x0F
    // 0x0F = 0000 1111 en binario
    // AND con 0x0F → fuerza PA4–PA7 a 0 (parte alta del byte)
    //                conserva PA0–PA3 tal como están (RS, RW, EN)
    // Esto limpia los bits de datos sin tocar los de control
    LCD_PORT &= 0x0F;

    // (nibble << 4)
    // Desplaza el nibble 4 posiciones a la izquierda
    // Ejemplo: nibble = 0b0101 → 0b01010000
    // Así los 4 bits de datos quedan en PA4–PA7
    // |= → OR para poner esos bits sin tocar PA0–PA3
    LCD_PORT |= (nibble << 4);

    // Genera el pulso EN para que el LCD lea los 4 bits
    pulso_en();
}


// ============================================================
// FUNCIONES PÚBLICAS (API)
// Declaradas en lcd.h — cualquier módulo puede llamarlas
// ============================================================

// ------------------------------------------------------------
// lcd_cmd(cmd)
// Envía un COMANDO al LCD (no un carácter a mostrar)
// Ejemplos de comandos: limpiar pantalla, mover cursor,
//                       configurar modo, etc.
// RS = 0 indica al LCD que lo que viene es un comando
// ------------------------------------------------------------
void lcd_cmd(uint8_t cmd) {

    // RS = 0 → modo comando
    // &= ~(1 << LCD_RS) → baja solo el bit RS (PA0) a 0
    LCD_PORT &= ~(1 << LCD_RS);

    // Enviar primero los 4 bits altos del comando
    // cmd >> 4 → desplaza 4 posiciones a la derecha
    // Ejemplo: cmd = 0b10110000 → cmd >> 4 = 0b00001011
    // Así el nibble alto queda en los 4 bits bajos
    // para que enviar_nibble lo coloque en PA4–PA7
    enviar_nibble(cmd >> 4);

    // Enviar los 4 bits bajos del comando
    // cmd & 0x0F → máscara que conserva solo los 4 bits bajos
    // Ejemplo: cmd = 0b10110111 → cmd & 0x0F = 0b00000111
    enviar_nibble(cmd & 0x0F);

    // Espera adicional para comandos lentos
    _delay_us(50);
}


// ------------------------------------------------------------
// lcd_char(c)
// Envía un CARÁCTER al LCD para que lo muestre en pantalla
// RS = 1 indica al LCD que lo que viene es un dato/carácter
// La lógica es idéntica a lcd_cmd pero con RS = 1
// ------------------------------------------------------------
void lcd_char(char c) {

    // RS = 1 → modo dato (carácter)
    // |= (1 << LCD_RS) → sube el bit RS (PA0) a 1
    LCD_PORT |= (1 << LCD_RS);

    // (uint8_t)c → cast a uint8_t para que el desplazamiento
    //              funcione correctamente con caracteres
    // >> 4 → nibble alto del carácter
    enviar_nibble((uint8_t)c >> 4);

    // & 0x0F → nibble bajo del carácter
    enviar_nibble((uint8_t)c & 0x0F);

    // Espera de proceso
    _delay_us(50);
}


// ------------------------------------------------------------
// lcd_string(s)
// Escribe una cadena de texto completa en el LCD
// Recorre el string carácter por carácter hasta encontrar
// el '\0' (terminador nulo que marca el fin del string en C)
// ------------------------------------------------------------
void lcd_string(const char* s) {

    // *s → accede al carácter que apunta el puntero s
    // Mientras el carácter no sea '\0' (fin del string)
    while (*s) {

        // Envía el carácter actual y avanza al siguiente
        // *s++ → primero usa *s (el carácter actual),
        //        luego incrementa el puntero s al siguiente
        lcd_char(*s++);
    }
}


// ------------------------------------------------------------
// lcd_goto(fila, col)
// Mueve el cursor a una posición específica del LCD
// fila: 0 = primera línea, 1 = segunda línea
// col:  0–15 (el LCD tiene 16 columnas)
// ------------------------------------------------------------
void lcd_goto(uint8_t fila, uint8_t col) {

    // El HD44780 usa direcciones DDRAM para las posiciones:
    // Línea 0 empieza en dirección 0x00 → comando 0x80 + col
    // Línea 1 empieza en dirección 0x40 → comando 0xC0 + col
    // El operador ternario elige la base según la fila
    uint8_t dir = (fila == 0) ? 0x80 : 0xC0;

    // Suma la columna a la dirección base y envía como comando
    // Ejemplo: fila=1, col=5 → 0xC0 + 5 = 0xC5
    lcd_cmd(dir + col);
}


// ------------------------------------------------------------
// lcd_clear()
// Borra todo el contenido del LCD y regresa el cursor
// a la posición (0,0) — esquina superior izquierda
// ------------------------------------------------------------
void lcd_clear() {

    // Comando 0x01 = "Clear Display" en el HD44780
    lcd_cmd(0x01);

    // Este comando es el más lento del LCD — necesita ~1.5ms
    // Se usa 2ms para asegurar que termina antes de continuar
    _delay_ms(2);
}


// ------------------------------------------------------------
// lcd_int(n)
// Escribe un número entero (positivo o negativo) en el LCD
// No existe una función nativa para esto en el HD44780 —
// hay que convertir el número a caracteres manualmente
// ------------------------------------------------------------
void lcd_int(int16_t n) {

    // Buffer temporal para guardar los dígitos
    // 7 posiciones: hasta 5 dígitos + signo + '\0'
    char buf[7];

    // i lleva la cuenta de cuántos dígitos se guardaron
    int8_t i = 0;

    // Si el número es negativo, mostrar el signo y trabajar
    // con el valor absoluto
    if (n < 0) {
        lcd_char('-');  // mostrar signo negativo
        n = -n;         // convertir a positivo
    }

    // Caso especial: si n es 0 mostrar '0' directamente
    // (el bucle while de abajo no entraría si n = 0)
    if (n == 0) {
        lcd_char('0');
        return;  // terminar la función aquí
    }

    // Extraer los dígitos de derecha a izquierda
    // n % 10 → obtiene el dígito menos significativo
    // '0' + dígito → convierte número a carácter ASCII
    //   Ejemplo: '0' + 3 = '3' (porque '0' = 48 en ASCII)
    // n /= 10 → elimina el dígito ya procesado
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    // Resultado: los dígitos quedaron guardados AL REVÉS
    // Ejemplo: n=123 → buf = ['3','2','1']

    // Imprimir los dígitos en orden inverso (de atrás hacia
    // adelante) para que salgan en el orden correcto
    // --i → decrementa i primero, luego lo usa como índice
    while (i > 0) {
        lcd_char(buf[--i]);
    }
}


// ------------------------------------------------------------
// lcd_init()
// Inicializa el LCD en modo 4 bits
// DEBE llamarse una sola vez al inicio del programa (setup)
// ANTES de cualquier otra función del LCD
// ------------------------------------------------------------
void lcd_init() {

    // Configurar Puerto A completo como SALIDA
    // 0xFF = 1111 1111 → todos los bits del DDR en 1 = salida
    LCD_DDR  = 0xFF;

    // Iniciar el puerto con todos los pines en 0 (LOW)
    LCD_PORT = 0x00;

    // Esperar 50ms para que el LCD complete su propio arranque
    // interno — el HD44780 necesita tiempo tras encender VCC
    _delay_ms(50);


    // ── SECUENCIA ESPECIAL DE INICIALIZACIÓN HD44780 ─────
    // El LCD arranca en modo 8 bits por defecto.
    // Para pasarlo a modo 4 bits hay que enviar la secuencia
    // exacta que indica el datasheet del HD44780.
    // Durante esta secuencia NO se usan los 8 bits completos
    // sino solo nibbles — por eso se llama enviar_nibble
    // directamente y no lcd_cmd.

    // RS = 0 durante toda la secuencia de inicialización
    LCD_PORT &= ~(1 << LCD_RS);

    // Intento 1: enviar nibble 0x3 = 0011
    // Le dice al LCD "function set en 8 bits" (aún en 8 bits)
    enviar_nibble(0x3);
    _delay_ms(5);       // esperar mínimo 4.1ms

    // Intento 2: repetir 0x3 — el datasheet lo exige
    enviar_nibble(0x3);
    _delay_us(150);     // esperar mínimo 100µs

    // Intento 3: repetir 0x3 por tercera vez
    enviar_nibble(0x3);
    _delay_us(50);

    // Ahora sí: enviar 0x2 = 0010
    // Esto le indica al HD44780 que cambie a modo 4 bits
    // A partir de aquí el LCD espera datos en nibbles
    enviar_nibble(0x2);
    _delay_us(50);


    // ── CONFIGURACIÓN DEL LCD ────────────────────────────
    // A partir de aquí ya podemos usar lcd_cmd normalmente
    // porque el LCD ya está en modo 4 bits

    // Comando 0x28 = 0010 1000
    // Bit 5 = 1 → Function Set
    // Bit 4 = 0 → modo 4 bits (DL=0)
    // Bit 3 = 1 → 2 líneas (N=1)
    // Bit 2 = 0 → matriz 5x8 puntos (F=0)
    lcd_cmd(0x28);

    // Comando 0x0C = 0000 1100
    // Bit 3 = 1 → Display Control
    // Bit 2 = 1 → Display ON (D=1)
    // Bit 1 = 0 → Cursor OFF (C=0)
    // Bit 0 = 0 → Blink OFF (B=0)
    lcd_cmd(0x0C);

    // Comando 0x06 = 0000 0110
    // Bit 2 = 1 → Entry Mode Set
    // Bit 1 = 1 → cursor avanza hacia la derecha (I/D=1)
    // Bit 0 = 0 → pantalla no se desplaza (S=0)
    lcd_cmd(0x06);

    // Limpiar la pantalla y colocar el cursor en (0,0)
    // Llama internamente a lcd_cmd(0x01) + delay de 2ms
    lcd_clear();
}


// ------------------------------------------------------------
// lcd_resync()
// Re-sincroniza el controlador HD44780 en modo 4 bits SIN borrar la pantalla
// ni hacer el wait de arranque. Sirve para RECUPERAR el LCD cuando el ruido
// (servo, RF, cables del entrenador) lo desincroniza y queda mostrando basura.
// No genera parpadeo (no usa clear). El contenido se repinta aparte.
// ------------------------------------------------------------
void lcd_resync() {

    LCD_DDR = 0xFF;                  // asegurar Puerto A como salida
    LCD_PORT &= ~(1 << LCD_RS);      // RS=0 -> modo comando

    // Secuencia del datasheet para realinear a 4 bits desde CUALQUIER estado
    // (recupera el desfase de nibbles que causa la "basura" en pantalla).
    enviar_nibble(0x3); _delay_us(200);
    enviar_nibble(0x3); _delay_us(200);
    enviar_nibble(0x3); _delay_us(200);
    enviar_nibble(0x2); _delay_us(200);

    lcd_cmd(0x28);   // Function set: 4 bits, 2 lineas, 5x8
    lcd_cmd(0x0C);   // Display ON, cursor OFF
    lcd_cmd(0x06);   // Entry mode: cursor incrementa
    // (sin lcd_clear -> sin parpadeo; el contenido se repinta despues)
}
