// lcd.cpp â€” implementaciÃ³n pendiente (Fase 1)
// Adaptar del taller HOLA MUNDO anterior
// ============================================================
// lcd.cpp â€” Driver LCD 16x2 HD44780 en modo 4 bits
// Puerto A del ATmega2560
// ============================================================

// avr/io.h   â†’ define los nombres de registros y puertos
//              (DDRA, PORTA, PA0, PA2, etc.)
#include <avr/io.h>

// util/delay.h â†’ permite usar _delay_ms() y _delay_us()
//                son delays precisos basados en F_CPU
#include <util/delay.h>

// lcd.h â†’ incluye las definiciones de pines y los prototipos
//          de las funciones pÃºblicas de este mÃ³dulo
#include "../include/lcd.h"


// ============================================================
// FUNCIONES INTERNAS (privadas)
// La palabra "static" hace que solo sean visibles dentro de
// este archivo .cpp â€” ningÃºn otro mÃ³dulo puede llamarlas
// ============================================================

// ------------------------------------------------------------
// pulso_en()
// Genera el pulso en el pin EN que le indica al LCD:
// "ahora lee lo que hay en los pines de datos"
// El HD44780 lee en el flanco de BAJADA del pin EN
// ------------------------------------------------------------
static void pulso_en() {

    // LCD_PORT |= (1 << LCD_EN)
    // |=  â†’ operaciÃ³n OR bit a bit para poner solo ese bit en 1
    //        sin modificar los demÃ¡s bits del puerto
    // (1 << LCD_EN) â†’ desplaza el valor 1 hasta la posiciÃ³n
    //                  del bit LCD_EN (PA2 = bit 2)
    // Resultado: EN sube a 1 (flanco de subida)
    LCD_PORT |=  (1 << LCD_EN);

    // Esperar 1 microsegundo â€” el HD44780 necesita que EN
    // permanezca en 1 al menos 450ns antes de bajar
    _delay_us(1);

    // LCD_PORT &= ~(1 << LCD_EN)
    // ~(1 << LCD_EN) â†’ invierte todos los bits: el bit LCD_EN
    //                   queda en 0 y todos los demÃ¡s en 1
    // &=  â†’ AND bit a bit: solo baja el bit LCD_EN, el resto
    //        no cambia
    // Resultado: EN baja a 0 (flanco de bajada â€” aquÃ­ el LCD
    //            captura el dato)
    LCD_PORT &= ~(1 << LCD_EN);

    // Esperar 50 microsegundos para que el LCD procese el dato
    // El HD44780 necesita mÃ­nimo 37Âµs para ejecutar un comando
    _delay_us(50);
}


// ------------------------------------------------------------
// enviar_nibble(nibble)
// Pone 4 bits en PA4â€“PA7 y genera el pulso EN
// En modo 4 bits los datos viajan por los 4 pines altos
// del puerto (PA4=D4, PA5=D5, PA6=D6, PA7=D7)
// ------------------------------------------------------------
static void enviar_nibble(uint8_t nibble) {

    // LCD_PORT &= 0x0F
    // 0x0F = 0000 1111 en binario
    // AND con 0x0F â†’ fuerza PA4â€“PA7 a 0 (parte alta del byte)
    //                conserva PA0â€“PA3 tal como estÃ¡n (RS, RW, EN)
    // Esto limpia los bits de datos sin tocar los de control
    LCD_PORT &= 0x0F;

    // (nibble << 4)
    // Desplaza el nibble 4 posiciones a la izquierda
    // Ejemplo: nibble = 0b0101 â†’ 0b01010000
    // AsÃ­ los 4 bits de datos quedan en PA4â€“PA7
    // |= â†’ OR para poner esos bits sin tocar PA0â€“PA3
    LCD_PORT |= (nibble << 4);

    // Genera el pulso EN para que el LCD lea los 4 bits
    pulso_en();
}


// ============================================================
// FUNCIONES PÃšBLICAS (API)
// Declaradas en lcd.h â€” cualquier mÃ³dulo puede llamarlas
// ============================================================

// ------------------------------------------------------------
// lcd_cmd(cmd)
// EnvÃ­a un COMANDO al LCD (no un carÃ¡cter a mostrar)
// Ejemplos de comandos: limpiar pantalla, mover cursor,
//                       configurar modo, etc.
// RS = 0 indica al LCD que lo que viene es un comando
// ------------------------------------------------------------
void lcd_cmd(uint8_t cmd) {

    // RS = 0 â†’ modo comando
    // &= ~(1 << LCD_RS) â†’ baja solo el bit RS (PA0) a 0
    LCD_PORT &= ~(1 << LCD_RS);

    // Enviar primero los 4 bits altos del comando
    // cmd >> 4 â†’ desplaza 4 posiciones a la derecha
    // Ejemplo: cmd = 0b10110000 â†’ cmd >> 4 = 0b00001011
    // AsÃ­ el nibble alto queda en los 4 bits bajos
    // para que enviar_nibble lo coloque en PA4â€“PA7
    enviar_nibble(cmd >> 4);

    // Enviar los 4 bits bajos del comando
    // cmd & 0x0F â†’ mÃ¡scara que conserva solo los 4 bits bajos
    // Ejemplo: cmd = 0b10110111 â†’ cmd & 0x0F = 0b00000111
    enviar_nibble(cmd & 0x0F);

    // Espera adicional para comandos lentos
    _delay_us(50);
}


// ------------------------------------------------------------
// lcd_char(c)
// EnvÃ­a un CARÃCTER al LCD para que lo muestre en pantalla
// RS = 1 indica al LCD que lo que viene es un dato/carÃ¡cter
// La lÃ³gica es idÃ©ntica a lcd_cmd pero con RS = 1
// ------------------------------------------------------------
void lcd_char(char c) {

    // RS = 1 â†’ modo dato (carÃ¡cter)
    // |= (1 << LCD_RS) â†’ sube el bit RS (PA0) a 1
    LCD_PORT |= (1 << LCD_RS);

    // (uint8_t)c â†’ cast a uint8_t para que el desplazamiento
    //              funcione correctamente con caracteres
    // >> 4 â†’ nibble alto del carÃ¡cter
    enviar_nibble((uint8_t)c >> 4);

    // & 0x0F â†’ nibble bajo del carÃ¡cter
    enviar_nibble((uint8_t)c & 0x0F);

    // Espera de proceso
    _delay_us(50);
}


// ------------------------------------------------------------
// lcd_string(s)
// Escribe una cadena de texto completa en el LCD
// Recorre el string carÃ¡cter por carÃ¡cter hasta encontrar
// el '\0' (terminador nulo que marca el fin del string en C)
// ------------------------------------------------------------
void lcd_string(const char* s) {

    // *s â†’ accede al carÃ¡cter que apunta el puntero s
    // Mientras el carÃ¡cter no sea '\0' (fin del string)
    while (*s) {

        // EnvÃ­a el carÃ¡cter actual y avanza al siguiente
        // *s++ â†’ primero usa *s (el carÃ¡cter actual),
        //        luego incrementa el puntero s al siguiente
        lcd_char(*s++);
    }
}


// ------------------------------------------------------------
// lcd_goto(fila, col)
// Mueve el cursor a una posiciÃ³n especÃ­fica del LCD
// fila: 0 = primera lÃ­nea, 1 = segunda lÃ­nea
// col:  0â€“15 (el LCD tiene 16 columnas)
// ------------------------------------------------------------
void lcd_goto(uint8_t fila, uint8_t col) {

    // El HD44780 usa direcciones DDRAM para las posiciones:
    // LÃ­nea 0 empieza en direcciÃ³n 0x00 â†’ comando 0x80 + col
    // LÃ­nea 1 empieza en direcciÃ³n 0x40 â†’ comando 0xC0 + col
    // El operador ternario elige la base segÃºn la fila
    uint8_t dir = (fila == 0) ? 0x80 : 0xC0;

    // Suma la columna a la direcciÃ³n base y envÃ­a como comando
    // Ejemplo: fila=1, col=5 â†’ 0xC0 + 5 = 0xC5
    lcd_cmd(dir + col);
}


// ------------------------------------------------------------
// lcd_clear()
// Borra todo el contenido del LCD y regresa el cursor
// a la posiciÃ³n (0,0) â€” esquina superior izquierda
// ------------------------------------------------------------
void lcd_clear() {

    // Comando 0x01 = "Clear Display" en el HD44780
    lcd_cmd(0x01);

    // Este comando es el mÃ¡s lento del LCD â€” necesita ~1.5ms
    // Se usa 2ms para asegurar que termina antes de continuar
    _delay_ms(2);
}


// ------------------------------------------------------------
// lcd_int(n)
// Escribe un nÃºmero entero (positivo o negativo) en el LCD
// No existe una funciÃ³n nativa para esto en el HD44780 â€”
// hay que convertir el nÃºmero a caracteres manualmente
// ------------------------------------------------------------
void lcd_int(int16_t n) {

    // Buffer temporal para guardar los dÃ­gitos
    // 7 posiciones: hasta 5 dÃ­gitos + signo + '\0'
    char buf[7];

    // i lleva la cuenta de cuÃ¡ntos dÃ­gitos se guardaron
    int8_t i = 0;

    // Si el nÃºmero es negativo, mostrar el signo y trabajar
    // con el valor absoluto
    if (n < 0) {
        lcd_char('-');  // mostrar signo negativo
        n = -n;         // convertir a positivo
    }

    // Caso especial: si n es 0 mostrar '0' directamente
    // (el bucle while de abajo no entrarÃ­a si n = 0)
    if (n == 0) {
        lcd_char('0');
        return;  // terminar la funciÃ³n aquÃ­
    }

    // Extraer los dÃ­gitos de derecha a izquierda
    // n % 10 â†’ obtiene el dÃ­gito menos significativo
    // '0' + dÃ­gito â†’ convierte nÃºmero a carÃ¡cter ASCII
    //   Ejemplo: '0' + 3 = '3' (porque '0' = 48 en ASCII)
    // n /= 10 â†’ elimina el dÃ­gito ya procesado
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    // Resultado: los dÃ­gitos quedaron guardados AL REVÃ‰S
    // Ejemplo: n=123 â†’ buf = ['3','2','1']

    // Imprimir los dÃ­gitos en orden inverso (de atrÃ¡s hacia
    // adelante) para que salgan en el orden correcto
    // --i â†’ decrementa i primero, luego lo usa como Ã­ndice
    while (i > 0) {
        lcd_char(buf[--i]);
    }
}


// ------------------------------------------------------------
// lcd_init()
// Inicializa el LCD en modo 4 bits
// DEBE llamarse una sola vez al inicio del programa (setup)
// ANTES de cualquier otra funciÃ³n del LCD
// ------------------------------------------------------------
void lcd_init() {

    // Configurar Puerto A completo como SALIDA
    // 0xFF = 1111 1111 â†’ todos los bits del DDR en 1 = salida
    LCD_DDR  = 0xFF;

    // Iniciar el puerto con todos los pines en 0 (LOW)
    LCD_PORT = 0x00;

    // Esperar 50ms para que el LCD complete su propio arranque
    // interno â€” el HD44780 necesita tiempo tras encender VCC
    _delay_ms(50);


    // â”€â”€ SECUENCIA ESPECIAL DE INICIALIZACIÃ“N HD44780 â”€â”€â”€â”€â”€
    // El LCD arranca en modo 8 bits por defecto.
    // Para pasarlo a modo 4 bits hay que enviar la secuencia
    // exacta que indica el datasheet del HD44780.
    // Durante esta secuencia NO se usan los 8 bits completos
    // sino solo nibbles â€” por eso se llama enviar_nibble
    // directamente y no lcd_cmd.

    // RS = 0 durante toda la secuencia de inicializaciÃ³n
    LCD_PORT &= ~(1 << LCD_RS);

    // Intento 1: enviar nibble 0x3 = 0011
    // Le dice al LCD "function set en 8 bits" (aÃºn en 8 bits)
    enviar_nibble(0x3);
    _delay_ms(5);       // esperar mÃ­nimo 4.1ms

    // Intento 2: repetir 0x3 â€” el datasheet lo exige
    enviar_nibble(0x3);
    _delay_us(150);     // esperar mÃ­nimo 100Âµs

    // Intento 3: repetir 0x3 por tercera vez
    enviar_nibble(0x3);
    _delay_us(50);

    // Ahora sÃ­: enviar 0x2 = 0010
    // Esto le indica al HD44780 que cambie a modo 4 bits
    // A partir de aquÃ­ el LCD espera datos en nibbles
    enviar_nibble(0x2);
    _delay_us(50);


    // â”€â”€ CONFIGURACIÃ“N DEL LCD â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    // A partir de aquÃ­ ya podemos usar lcd_cmd normalmente
    // porque el LCD ya estÃ¡ en modo 4 bits

    // Comando 0x28 = 0010 1000
    // Bit 5 = 1 â†’ Function Set
    // Bit 4 = 0 â†’ modo 4 bits (DL=0)
    // Bit 3 = 1 â†’ 2 lÃ­neas (N=1)
    // Bit 2 = 0 â†’ matriz 5x8 puntos (F=0)
    lcd_cmd(0x28);

    // Comando 0x0C = 0000 1100
    // Bit 3 = 1 â†’ Display Control
    // Bit 2 = 1 â†’ Display ON (D=1)
    // Bit 1 = 0 â†’ Cursor OFF (C=0)
    // Bit 0 = 0 â†’ Blink OFF (B=0)
    lcd_cmd(0x0C);

    // Comando 0x06 = 0000 0110
    // Bit 2 = 1 â†’ Entry Mode Set
    // Bit 1 = 1 â†’ cursor avanza hacia la derecha (I/D=1)
    // Bit 0 = 0 â†’ pantalla no se desplaza (S=0)
    lcd_cmd(0x06);

    // Limpiar la pantalla y colocar el cursor en (0,0)
    // Llama internamente a lcd_cmd(0x01) + delay de 2ms
    lcd_clear();
}
