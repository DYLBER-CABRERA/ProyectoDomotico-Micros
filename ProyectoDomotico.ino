// ProyectoDomotico.ino - Archivo principal del sistema domotico
// Fases activas: 1 (LCD) + 2 (Teclado) + 3 (USART) + 4 (RFID+Acceso) +
//                5 (Alarma) + 6 (Motor+Temp+Dimmer) + 7 (Horno+Sonido+Mercado)
// Dimmer ajustado a escala 0-10 (limitacion visual confirmada en Proteus)
//
// LAYOUT DEL LCD (16 columnas x 2 filas):
//   Linea 0 (fila 0): SIEMPRE el estado de la alarma (DESACTIV/ARMADA/INCENDIO/INTRUSION)
//   Linea 1 (fila 1): dividida en dos mitades
//     - Columnas 0-8:  temperatura ("T:25C")
//     - Columnas 9-15: nivel de luz ("L:7/10")
//   Mientras se ingresa el codigo de alarma, "Codigo: ****" usa toda la linea 1
//   (es mas largo que el espacio de temperatura+luz, por eso las pisa
//    temporalmente mientras el usuario esta digitando)


// ─── INCLUDES ────────────────────────────────────────────────────────────────

// string.h: provee strncmp() para comparar N caracteres de dos cadenas sin copiarlas.
//   Se usa para parsear prefijos de comandos: "ARM", "DISARM:", "LUZ:", "GARAJE:ABRIR".
#include <string.h>

// ctype.h: provee toupper() para convertir un caracter a mayuscula.
//   Necesario en a_mayusculas() para que los comandos funcionen en cualquier casing.
#include <ctype.h>

// util/delay.h: provee _delay_ms() y _delay_us() con tiempos precisos basados en F_CPU.
//   Usados en motor.cpp (delay entre pasos del PAP) y lcd.cpp (tiempos del HD44780).
#include <util/delay.h>

// Modulos activos del proyecto � cada header expone solo la API publica del modulo
#include "include/lcd.h"         // lcd_init, lcd_goto, lcd_string, lcd_int, lcd_char, lcd_clear
#include "include/teclado.h"     // teclado_init, teclado_hay, teclado_leer
#include "include/usart.h"       // usart_init, usart_enviar_*, usart_hay_linea, usart_leer_linea
#include "include/alarma.h"      // alarma_init/armar/desarmar/actualizar, ALARMA_TIPO_*, ALARMA_CODIGO_LEN
#include "include/adc.h"         // adc_init, adc_leer (ADC compartido: temperatura y volumen)
#include "include/motor.h"       // motor_init, garaje_abrir, garaje_cerrar (servo)
#include "include/temperatura.h" // temp_init, temp_celsius, temp_controlar
#include "include/dimmer.h"      // dimmer_init, dimmer_set, dimmer_get, DIMMER_NIVEL_MAX
#include "include/horno.h"
#include "include/sonido.h"
#include "include/mercado.h"
// Fase 4: bus SPI para el modulo RC522 RFID
#include "include/spi_master.h"
// Fase 4: lectura de tarjetas RFID
#include "include/rfid.h"
// Fase 4: UIDs autorizados en EEPROM interna
#include "include/eeprom_mgr.h"
// Fase 4: logica de acceso por tarjeta
#include "include/acceso.h"


// ─── VARIABLES Y CONSTANTES DEL MODULO ──────────────────────────────────────
// 'static' en el ambito de archivo hace las variables PRIVADAS de este .ino:
// ningun otro modulo puede acceder ni modificar estas variables directamente.

// Buffer que acumula los digitos del codigo de alarma mientras el usuario los teclea.
// Tamano = ALARMA_CODIGO_LEN (4 digitos) + 1 para el terminador nulo '\0'.
// El terminador es obligatorio para que alarma_verificar_codigo() lo trate como string C.
static char    buffer_codigo[ALARMA_CODIGO_LEN + 1];

// Indice de escritura en buffer_codigo: indica cuantos digitos se han ingresado.
// Rango valido: 0 (buffer vacio) ... ALARMA_CODIGO_LEN (buffer lleno, listo para validar).
// uint8_t: entero de 8 bits sin signo (0-255), suficiente para un indice de 0 a 4.
static uint8_t pos_codigo = 0;

// Marca de tiempo (ms) de la ultima lectura de temperatura. Ahora se usa TIEMPO
// REAL (millis_sistema, base Timer2) en vez de "vueltas del loop", porque con el
// RFID el loop se volvio lento y variable y la temperatura llegaba tardisimo.
static uint32_t ultima_lectura_temp = 0;

// Cada cuantos MILISEGUNDOS se relee el sensor y se actualizan calefactor/ventilador.
#define INTERVALO_TEMP  1000  // 1 s -> sigue siendo responsivo y escribe menos al LCD

// Marca de tiempo (ms) de la ultima interaccion activa del usuario.
// "Interaccion" = pulsacion de cualquier tecla, comando LUZ:, o evento RFID.
// PROPOSITO: evitar que la temperatura pise el mensaje de codigo/RFID en el LCD
// mientras el usuario esta interactuando. Se actualiza a millis_sistema() en esos
// eventos. Inicia en 0: la temperatura aparece ~TIEMPO_MOSTRAR_INTERACCION tras el arranque.
static uint32_t ultima_interaccion = 0;

// Milisegundos sin interaccion antes de que la temperatura pueda actualizar el LCD.
// Si (millis - ultima_interaccion) >= este valor -> linea libre -> mostrar temp.
#define TIEMPO_MOSTRAR_INTERACCION  3000  // 3 s


// ─── MODO COMANDO POR TECLADO (entrenador sin terminal serial) ──────────────
// En el entrenador fisico solo hay teclado: no hay terminal para escribir
// "SONIDO:ON,75". Por eso se agrega un "modo comando" que se activa con la
// tecla '*', se teclea un codigo de funcion de 2 digitos + parametros (la
// tecla 'A' separa parametros) y se ejecuta con '#'. La tecla '*' tambien
// cancela. Ejemplo: encender sonido al 75% -> * 2 1 7 5 #
// Todas las combinaciones estan documentadas en COMANDOS_TECLADO.md.

// 1 = estamos capturando un comando por teclado; 0 = modo normal (codigo alarma)
static uint8_t modo_comando = 0;

// Buffer que acumula las teclas del comando (codigo + parametros + separadores 'A')
static char    cmd_buffer[16];

// Indice de escritura en cmd_buffer
static uint8_t cmd_pos = 0;

// Declaracion adelantada: la implementacion esta despues de texto_a_numero()
// (que este ejecutor necesita para convertir los parametros).
static void ejecutar_comando_teclado(char* cmd);


// ─── LETRERO DESLIZANTE DEL MERCADO (marquee de la linea 0) ──────────────────
// Cuando se agrega o elimina un producto se muestra "<producto> AGREGADO" o
// "<producto> ELIMINADO" en la linea 0. Si el texto cabe en 16 columnas se
// muestra FIJO; si es mas largo, se desliza hacia la izquierda como letrero de
// bus. Es NO BLOQUEANTE: marquee_actualizar() avanza un paso por vuelta del loop
// solo cuando toca, sin frenar teclado/alarmas/serial.
static char     marquee_buf[40];     // mensaje completo a mostrar
static uint8_t  marquee_len    = 0;  // largo del mensaje en marquee_buf
static uint8_t  marquee_off    = 0;  // inicio de la ventana visible (desplazamiento)
static uint8_t  marquee_activo = 0;  // 1 = hay un mensaje deslizandose
static uint16_t marquee_tick   = 0;  // vueltas del loop desde el ultimo paso

// Vueltas del loop entre cada desplazamiento de 1 caracter. Mas alto = mas lento.
// Referencia: INTERVALO_TEMP = 2000 equivale a ~2-3 s, asi que 300 da ~0.3-0.4 s
// por paso. Ajusta a gusto si lo quieres mas rapido o mas lento.
#define MARQUEE_VELOCIDAD  300


// ─── SELECTOR DE LUGAR (2 pulsadores) + RETARDO DE DESARME ───────────────────
// Los 2 pulsadores alternan entre los 3 lugares de acceso (lista circular):
//   PUERTA (0) -> GARAJE (1) -> SALA (2) -> PUERTA (0) ...
//   Izquierdo -> D40 (PG1): retrocede   |   Derecho -> D39 (PG2): avanza
// El RFID lee el lugar con obtener_lugar() y actua segun corresponda.
static uint8_t  lugar_actual  = LUGAR_PUERTA;  // arranca en puerta principal
static uint8_t  btn_izq_prev  = 0;             // estado anterior (0=suelto; activo en ALTO)
static uint8_t  btn_der_prev  = 0;
static uint32_t btn_ultimo_ms = 0;             // antirrebote de los pulsadores
#define BTN_DEBOUNCE_MS  200

// Retardo de desarme: tras conceder acceso en puerta/garaje, si la alarma esta
// ARMADA, hay N segundos para desarmar con el codigo; si no, se dispara intruso.
static uint8_t  retardo_activo   = 0;
static uint32_t retardo_inicio   = 0;
static uint32_t retardo_total_ms = 0;
static uint8_t  retardo_ult_seg  = 0;   // ultimo segundo mostrado (evita repintar)

// Auto-recuperacion del LCD: cada cuanto (ms) re-sincronizar + repintar la
// pantalla estandar EN REPOSO. Si el ruido del entrenador corrompe el LCD, se
// recupera solo en ~2 s. Subelo si quieres menos escrituras; baja-lo si quieres
// que se recupere mas rapido.
static uint32_t ultima_resync_lcd = 0;
#define RESYNC_LCD_MS  2000


// ─────────────────────────────────────────────────────────────────────────────
// a_mayusculas(s)
// Convierte la cadena 's' a mayusculas en el lugar (modifica el original).
// RAZON: strncmp() en procesar_comando_serial() es case-sensitive.
//        Sin esta conversion, "arm", "Arm" y "ARM" serian cadenas distintas.
//        Al normalizar antes de comparar, se acepta cualquier casing del usuario.
// PARAMETRO: char* s — puntero al string a convertir (debe ser mutable, no literal).
// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
// rfid_notifica_lcd()
// La llama el modulo RFID despues de escribir un mensaje en el LCD (acceso,
// enrolamiento, etc.). Reinicia ultima_interaccion para que la temperatura NO
// pise ese mensaje de inmediato y se pueda leer unos segundos en la pantalla.
// (No es static: el modulo rfid.cpp la usa por 'extern'.)
// ─────────────────────────────────────────────────────────────────────────────
void rfid_notifica_lcd() {
    ultima_interaccion = millis_sistema();
}


// ─────────────────────────────────────────────────────────────────────────────
// obtener_lugar()
// Devuelve el lugar de acceso seleccionado con los 2 pulsadores. La usa rfid.cpp
// (por 'extern') para decidir que hacer cuando se presenta una tarjeta valida.
// ─────────────────────────────────────────────────────────────────────────────
uint8_t obtener_lugar() {
    return lugar_actual;
}

// ─────────────────────────────────────────────────────────────────────────────
// acceso_iniciar_retardo(segundos)
// La llama rfid.cpp tras conceder acceso en puerta/garaje. SOLO arranca el
// retardo si la alarma de acceso esta ARMADA (si esta desarmada, no hay nada
// que desarmar: el acceso ya quedo concedido). El loop vigila el vencimiento.
// ─────────────────────────────────────────────────────────────────────────────
void acceso_iniciar_retardo(uint8_t segundos) {
    if (alarma_estado() == ALARMA_ARMADA) {
        retardo_activo   = 1;
        retardo_inicio   = millis_sistema();
        retardo_total_ms = (uint32_t)segundos * 1000;
        retardo_ult_seg  = 0xFF;   // forzar el primer repintado del contador
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// mostrar_lugar()
// Escribe en la linea 0 el lugar seleccionado (al pulsar un boton) y lo reporta
// por serial. Linea 0 = no choca con la temperatura (linea 1).
// ─────────────────────────────────────────────────────────────────────────────
static void mostrar_lugar() {
    lcd_goto(0, 0);
    if      (lugar_actual == LUGAR_PUERTA) lcd_string("LUGAR: PUERTA   ");
    else if (lugar_actual == LUGAR_GARAJE) lcd_string("LUGAR: GARAJE   ");
    else                                   lcd_string("LUGAR: SALA     ");
    usart_enviar_string("LUGAR:");
    usart_enviar_int(lugar_actual);
    usart_enviar_newline();
}


static void a_mayusculas(char* s) {

    // while (*s): desreferencia el puntero y evalua el caracter apuntado.
    // El bucle continua mientras el caracter NO sea '\0' (fin del string en C).
    // Cuando *s == '\0', el cast booleano es 0 (falso) y el bucle termina.
    while (*s) {

        // toupper(): convierte el caracter a mayuscula si es letra minuscula.
        //   Si ya es mayuscula, numero o simbolo, lo devuelve sin cambio.
        // (unsigned char)*s: cast OBLIGATORIO por el estandar C.
        //   En AVR-GCC 'char' es signed (-128..127). Si un caracter tiene el
        //   bit 7 en 1 (ej. vocales acentuadas: 'a'=0xE1=225, interpretado
        //   como -31 en signed), pasarlo a toupper() con valor negativo es
        //   comportamiento indefinido. El cast a unsigned char (0-255) lo hace seguro.
        // *s = ...: escribe el resultado de vuelta en la misma posicion del string.
        //   El puntero 's' sigue apuntando al mismo lugar; solo se modifica el VALOR.
        *s = toupper((unsigned char)*s);

        // s++: aritmetica de punteros — avanza el puntero 1 posicion (1 byte para char).
        // Ahora *s apunta al siguiente caracter del string.
        s++;
    }
    // Al salir del while, s apunta al '\0'. El string original quedo modificado.
}


// ─────────────────────────────────────────────────────────────────────────────
// reportar_codigo_incorrecto()
// Maneja de forma centralizada un intento de codigo EQUIVOCADO (al armar o
// desarmar, por teclado o serial). Cuenta el fallo en el modulo de alarma:
//   - Si aun no se alcanza el umbral (ALARMA_MAX_INTENTOS): muestra
//     "CODIGO INCORRECTO" y responde "ERROR:CODIGO".
//   - Si este fallo alcanza el umbral: alarma_registrar_fallo() dispara la
//     alarma de INTRUSO (LED rojo) y aqui se muestra "!! INTRUSO !!" y se
//     envia "ALARMA:INTRUSO" por serial.
// NO emite el newline final: lo agrega el llamador (igual que en los bloques OK).
// ─────────────────────────────────────────────────────────────────────────────
static void reportar_codigo_incorrecto() {

    if (alarma_registrar_fallo()) {
        // Se alcanzo el numero maximo de intentos -> intento de intrusion
        lcd_goto(0, 0);
        lcd_string("!! INTRUSO !!   ");
        usart_enviar_string("ALARMA:INTRUSO");
    } else {
        // Aun quedan intentos: solo avisar del error de codigo
        lcd_goto(0, 0);
        lcd_string("CODIGO INCORRECTO");
        usart_enviar_string("ERROR:CODIGO");
    }
}


// ─────────────────────────────────────────────────────────────────────────────
// procesar_tecla_alarma(tecla)
// Interpreta la tecla pulsada en el teclado fisico y ejecuta la accion correspondiente.
//
// Mapa de teclas:
//   0-9  → acumular digito en el buffer del codigo de alarma
//   A    → ARMAR la alarma de acceso (requiere codigo completo y correcto)
//   B    → DESARMAR la alarma de acceso (requiere codigo correcto)
//   C    → subir nivel del dimmer en 1 (max DIMMER_NIVEL_MAX = 10)
//   D    → bajar nivel del dimmer en 1 (min 0)
// ─────────────────────────────────────────────────────────────────────────────
static void procesar_tecla_alarma(char tecla) {

    // ── TECLA '*': INICIAR / CANCELAR MODO COMANDO ───────────────────────────
    // '*' es el prefijo universal de comandos por teclado (ver COMANDOS_TECLADO.md).
    // Si ya estabamos en modo comando, '*' cancela y vuelve a modo normal.
    if (tecla == '*') {
        ultima_interaccion = millis_sistema();
        if (modo_comando) {
            modo_comando = 0;
            lcd_goto(0, 0);   // CMD en linea 0 para NO chocar con la temperatura (linea 1)
            lcd_string("CMD CANCELADO   ");
        } else {
            modo_comando = 1;
            cmd_pos = 0;
            cmd_buffer[0] = '\0';
            lcd_goto(0, 0);   // CMD en linea 0 (no choca con temperatura)
            lcd_string("CMD:            "); // pista visual de que se espera el comando
        }
        return;
    }

    // ── EN MODO COMANDO: acumular digitos / 'A' (separador) y ejecutar con '#'
    if (modo_comando) {
        ultima_interaccion = millis_sistema();

        // '#': ejecutar el comando acumulado y salir del modo comando
        if (tecla == '#') {
            modo_comando = 0;
            lcd_goto(0, 0);   // limpiar el CMD de la linea 0 antes de ejecutar
            lcd_string("                "); // borrar el eco "CMD:31180A6..."
            ejecutar_comando_teclado(cmd_buffer);
            return;
        }

        // Solo se aceptan digitos 0-9 y la tecla 'A' (separador de parametros).
        // B, C y D se ignoran dentro del modo comando.
        if ((tecla >= '0' && tecla <= '9') || tecla == 'A') {
            if (cmd_pos < sizeof(cmd_buffer) - 1) {
                cmd_buffer[cmd_pos++] = tecla;
                cmd_buffer[cmd_pos]   = '\0';
            }
            // Eco en el LCD (linea 0) para que el usuario vea lo que lleva tecleado
            lcd_goto(0, 0);
            lcd_string("CMD:");
            lcd_string(cmd_buffer);
            for (uint8_t i = 4 + cmd_pos; i < 16; i++) lcd_char(' '); // limpiar resto
        }
        return;
    }

    // ── MODO NORMAL ──────────────────────────────────────────────────────────
    // Cualquier tecla pulsada cuenta como "interaccion activa".
    // Reiniciar el contador a 0 para que la temperatura NO pise la linea 1 del LCD
    // mientras el usuario esta en medio de digitar un codigo o ajustar el dimmer.
    ultima_interaccion = millis_sistema();

    // ── DIGITOS 0-9 ─────────────────────────────────────────────────────────
    // tecla >= '0' && tecla <= '9': comparacion con los codigos ASCII de los digitos.
    // '0'=48, '9'=57 — el teclado entrega el caracter ASCII, no el valor numerico.
    // Se evaluan primero para que el 'return' al final evite caer en los bloques A/B/C/D.
    if (tecla >= '0' && tecla <= '9') {

        // Solo aceptar si el buffer no esta lleno (menos de 4 digitos ingresados).
        // Ignorar el digito en silencio si el buffer ya tiene ALARMA_CODIGO_LEN caracteres.
        if (pos_codigo < ALARMA_CODIGO_LEN) {

            // Guardar el caracter del digito en la posicion actual del buffer.
            // buffer_codigo[pos_codigo] = tecla: escribe el caracter en el indice actual.
            // pos_codigo++: incrementa el indice DESPUES de usarlo (post-incremento).
            buffer_codigo[pos_codigo++] = tecla;

            // Agregar el terminador nulo en la nueva posicion (pos_codigo ya fue incrementado).
            // '\0' marca el fin del string en C — sin esto, funciones como strcmp
            // y alarma_verificar_codigo() leeran basura de memoria mas alla del codigo.
            buffer_codigo[pos_codigo]   = '\0';

            // Mostrar el codigo enmascarado en la LINEA 0 (asi no choca con la
            // temperatura/luz de la linea 1, que era el problema reportado).
            lcd_goto(0, 0);               // linea 0, columna 0
            lcd_string("Codigo: ");       // prefijo fijo de 8 caracteres

            // Mostrar un '*' por cada digito ya ingresado (enmascarar para seguridad).
            // i < pos_codigo: el bucle recorre de 0 hasta el ultimo digito ingresado.
            for (uint8_t i = 0; i < pos_codigo; i++) lcd_char('*');

            // Mostrar un '_' por cada posicion aun vacia (indicador visual del largo esperado).
            // i va desde la posicion actual hasta el maximo: los 4 - ya ingresados = vacios.
            for (uint8_t i = pos_codigo; i < ALARMA_CODIGO_LEN; i++) lcd_char('_');

            // Limpiar posibles caracteres sobrantes de mensajes anteriores mas largos.
            // "    " (4 espacios): borra restos que pudieran quedar a la derecha.
            lcd_string("    ");
        }

        // return: salir de la funcion sin evaluar los bloques A/B/C/D.
        // Un digito nunca debe interpretarse como tecla de funcion.
        return;
    }

    // ── TECLA A: ARMAR alarma de acceso ──────────────────────────────────────
    if (tecla == 'A') {

        // Condicion doble con &&:
        //   1. pos_codigo == ALARMA_CODIGO_LEN: el usuario ingreso exactamente 4 digitos.
        //   2. alarma_verificar_codigo(buffer_codigo): el codigo ingresado es correcto.
        // Si cualquiera de las dos falla, se muestra error — no se puede armar sin codigo completo.
        if (pos_codigo == ALARMA_CODIGO_LEN && alarma_verificar_codigo(buffer_codigo)) {
            alarma_armar();               // habilitar INT2/INT3 y encender LED verde
            alarma_reset_intentos();      // codigo correcto: limpiar contador anti-intrusos
            lcd_goto(0, 0);              // ir a linea 0 (exclusiva del estado de alarma)
            lcd_string("ALARMA: ARMADA  "); // 16 caracteres exactos — llena la linea completa
            usart_enviar_string("OK:ARMADA"); // confirmar por serial
        } else {
            // Codigo equivocado: contar el fallo (puede disparar la alarma de intruso)
            reportar_codigo_incorrecto();
        }
        usart_enviar_newline(); // \r\n al final del mensaje serial (requerido por terminales Windows)

        // Limpiar el buffer del codigo despues de cada intento (exitoso o fallido).
        // Asi el proximo intento siempre empieza desde cero.
        pos_codigo = 0;           // reiniciar el indice de escritura
        buffer_codigo[0] = '\0';  // marcar el buffer como string vacio

    // ── TECLA B: DESARMAR alarma de acceso ───────────────────────────────────
    } else if (tecla == 'B') {

        // Misma logica de validacion que tecla A: codigo completo Y correcto.
        if (pos_codigo == ALARMA_CODIGO_LEN && alarma_verificar_codigo(buffer_codigo)) {
            alarma_desarmar();            // deshabilitar INT2/INT3, apagar LEDs rojo y verde
            alarma_reset_intentos();      // codigo correcto: limpiar contador anti-intrusos
            lcd_goto(0, 0);
            lcd_string("ALARMA: DESACTIV"); // 16 caracteres — llena la linea 0 completamente
            usart_enviar_string("OK:DESACTIVADA");
        } else {
            // Codigo equivocado: contar el fallo (puede disparar la alarma de intruso)
            reportar_codigo_incorrecto();
        }
        usart_enviar_newline();

        // Limpiar buffer en cualquier caso (exito o error)
        pos_codigo = 0;
        buffer_codigo[0] = '\0';

    // ── TECLA C: SUBIR nivel del dimmer en 1 ─────────────────────────────────
    } else if (tecla == 'C') {

        // Leer el nivel actual y sumarle 1 para calcular el nuevo valor.
        // Se usa una variable temporal 'nuevo_nivel' para poder clampear antes de aplicar.
        uint8_t nuevo_nivel = dimmer_get() + 1;

        // Si supera el maximo (10), saturar en el maximo. Evita desbordamiento de uint8_t.
        // Sin este control, nivel 10 + 1 = 11, que es mayor que DIMMER_NIVEL_MAX.
        if (nuevo_nivel > DIMMER_NIVEL_MAX) nuevo_nivel = DIMMER_NIVEL_MAX;

        dimmer_set(nuevo_nivel); // aplicar el nuevo nivel al registro OCR1A del Timer1

        // Mostrar el nivel de luz en la mitad DERECHA de la linea 1 (columnas 9-15).
        // lcd_goto(1, 9): linea 1, columna 9 — no pisa la temperatura en columnas 0-8.
        lcd_goto(1, 9);
        lcd_string("       ");   // 7 espacios: borra TODO lo que había antes en esa zona
        lcd_goto(1, 9);           // volver al inicio de esa zona
        lcd_string("L:");
        lcd_int(dimmer_get());
        lcd_string("/10");     // denominador fijo para indicar la escala

        // Reportar el cambio por serial para que la aplicacion remota sepa el nuevo nivel
        usart_enviar_string("LUZ:");
        usart_enviar_int(dimmer_get()); // enviar el numero como texto ASCII (ej. "7")
        usart_enviar_newline();

    // ── TECLA D: BAJAR nivel del dimmer en 1 ─────────────────────────────────
    } else if (tecla == 'D') {

        // int16_t en lugar de uint8_t para poder detectar el caso nivel=0 → nivel-1=-1.
        // Con uint8_t, 0-1 daria 255 (desbordamiento de entero sin signo) en lugar de -1.
        // int16_t permite el valor negativo, que luego se clampea a 0.
        int16_t nuevo_nivel = (int16_t)dimmer_get() - 1;

        // Clampear al minimo 0: si el resultado fue negativo, forzarlo a 0.
        if (nuevo_nivel < 0) nuevo_nivel = 0;

        // (uint8_t): cast de vuelta a uint8_t — ya sabemos que el valor es 0-9 aqui,
        // el cast es seguro y dimmer_set() espera uint8_t.
        dimmer_set((uint8_t)nuevo_nivel);

        lcd_goto(1, 9);
        lcd_string("       ");   // 7 espacios: borra TODO lo que había antes en esa zona
        lcd_goto(1, 9);           // volver al inicio de esa zona
        lcd_string("L:");
        lcd_int(dimmer_get());
        lcd_string("/10");

        usart_enviar_string("LUZ:");
        usart_enviar_int(dimmer_get());
        usart_enviar_newline();
    }
    // Si la tecla no es 0-9 ni A/B/C/D, se ignora silenciosamente.
    // (teclas * y # del teclado no tienen funcion asignada en esta fase)
}


// ─────────────────────────────────────────────────────────────────────────────
// texto_a_numero(s)
// Convierte una cadena de digitos ASCII a su valor numerico como uint8_t.
// Reemplazo de atoi() de <stdlib.h> para mantener el proyecto sin dependencias
// de la libreria estandar (ahorra ~2KB de Flash en la tabla de conversion).
//
// ALGORITMO DE HORNER: procesa los digitos de izquierda a derecha.
//   Ejemplo con "42":
//     resultado = 0*10 + ('4'-'0') = 4
//     resultado = 4*10 + ('2'-'0') = 42
//
// El bucle se detiene en el primer caracter no-digito ('\0', espacio, etc.),
// lo que permite pasar un puntero al medio de un string: texto_a_numero(cmd + 4).
//
// LIMITE: uint8_t maximo = 255. Numeros mayores se truncan silenciosamente.
//         Para LUZ:0-10 y GARAJE esto no es un problema, pero el llamador
//         debe ser consciente del limite si se usa para temperaturas altas.
// ─────────────────────────────────────────────────────────────────────────────
static uint8_t texto_a_numero(const char* s) {

    uint8_t resultado = 0; // acumulador inicializado en 0 antes de procesar digitos

    // *s >= '0' && *s <= '9': verifica que el caracter actual sea un digito ASCII.
    // '0'=48, '9'=57 en ASCII. El bucle se rompe ante cualquier otro caracter.
    while (*s >= '0' && *s <= '9') {

        // resultado * 10: desplazar el valor acumulado una posicion decimal a la izquierda.
        // *s - '0': convertir el caracter ASCII al valor numerico.
        //   Ejemplo: '7' - '0' = 55 - 48 = 7.
        // Sumar ambos: agregar el nuevo digito como unidades.
        resultado = (resultado * 10) + (*s - '0');

        s++; // avanzar al siguiente caracter del string
    }

    return resultado; // retornar el valor numerico convertido
}


// ─────────────────────────────────────────────────────────────────────────────
// procesar_comando_serial(comando)
// Parsea y ejecuta el comando recibido por USART0 (ya convertido a mayusculas).
//
// PROTOCOLO (separar cadena con prefijos fijos):
//   "ARM:xxxx"       → armar alarma de acceso (requiere codigo, RF-04)
//   "DISARM:xxxx"    → desarmar con codigo de 4 digitos
//   "LUZ:N"          → ajustar dimmer a nivel N (0-10)
//   "GARAJE:ABRIR"   → abrir garaje (motor PAP horario)
//   "GARAJE:CERRAR"  → cerrar garaje (motor PAP antihorario)
//
// strncmp(s1, s2, n): compara los primeros n caracteres de s1 y s2.
//   Retorna 0 si son iguales, != 0 si difieren.
//   Se usa 'n' igual al largo del prefijo para no necesitar que el comando
//   termine exactamente ahi (util para comandos con argumentos como "LUZ:7").
// ─────────────────────────────────────────────────────────────────────────────
static void procesar_comando_serial(char* comando) {

    // ── ARM:xxxx ─────────────────────────────────────────────────────────────
    // El enunciado exige que la alarma se ACTIVE y DESACTIVE solo con codigo.
    // Por eso armar por serial requiere el codigo: "ARM:1234" (igual que la
    // tecla 'A' del teclado, que tambien valida el codigo antes de armar).
    // strncmp(..., "ARM:", 4): verificar el prefijo de 4 caracteres "ARM:".
    if (strncmp(comando, "ARM:", 4) == 0) {

        // El codigo viene justo despues de "ARM:" (comando+4 apunta a "1234").
        char* codigo_recibido = comando + 4;

        if (alarma_verificar_codigo(codigo_recibido)) {
            alarma_armar();          // habilitar INT2/INT3 y encender LED verde (PB6)
            alarma_reset_intentos(); // codigo correcto: limpiar contador anti-intrusos
            lcd_goto(0, 0);         // linea 0: exclusiva del estado de alarma
            lcd_string("ALARMA: ARMADA  "); // 16 chars exactos para llenar la linea
            usart_enviar_string("OK:ARMADA");
        } else {
            // Codigo incorrecto: contar el fallo (puede disparar la alarma de intruso)
            reportar_codigo_incorrecto();
        }
        usart_enviar_newline(); // \r\n — necesario para que la terminal muestre nueva linea

    // ── DISARM:xxxx ──────────────────────────────────────────────────────────
    // strncmp(..., "DISARM:", 7): verificar el prefijo de 7 caracteres "DISARM:".
    } else if (strncmp(comando, "DISARM:", 7) == 0) {

        // Extraer el codigo que viene inmediatamente despues del prefijo "DISARM:".
        // 'comando + 7': aritmetica de punteros — avanza el puntero 7 posiciones.
        //   Si comando apunta a "DISARM:1234", entonces comando+7 apunta a "1234".
        //   No se copia el string, solo se obtiene un puntero a la parte relevante.
        char* codigo_recibido = comando + 7;

        // Verificar el codigo antes de desarmar.
        // alarma_verificar_codigo() compara byte a byte contra el codigo guardado en Flash.
        if (alarma_verificar_codigo(codigo_recibido)) {
            alarma_desarmar();           // deshabilitar INT2/INT3, apagar LEDs rojo y verde
            alarma_reset_intentos();     // codigo correcto: limpiar contador anti-intrusos
            lcd_goto(0, 0);
            lcd_string("ALARMA: DESACTIV"); // 16 chars — llena la linea 0
            usart_enviar_string("OK:DESACTIVADA");
        } else {
            // Codigo incorrecto: contar el fallo (puede disparar la alarma de intruso)
            reportar_codigo_incorrecto();
        }
        usart_enviar_newline();

    // ── LUZ:N ────────────────────────────────────────────────────────────────
    // strncmp(..., "LUZ:", 4): verificar el prefijo de 4 caracteres "LUZ:".
    } else if (strncmp(comando, "LUZ:", 4) == 0) {

        // Escala 0-10 por limitacion visual de Proteus (ver dimmer.cpp para explicacion).
        // 'comando + 4': saltar los 4 caracteres de "LUZ:" y apuntar al numero.
        //   Ejemplo: "LUZ:7" → comando+4 apunta a "7" → texto_a_numero retorna 7.
        uint8_t nivel = texto_a_numero(comando + 4);

        dimmer_set(nivel); // aplicar el nivel al Timer1 OCR1A

        // Marcar interaccion: evitar que la temperatura pise el nivel de luz en LCD.
        // El usuario acaba de ajustar la luz — debe poder verla al menos TIEMPO_MOSTRAR
        // ciclos antes de que T: vuelva a aparecer en la misma linea.
        ultima_interaccion = millis_sistema();

        // Mostrar en la mitad DERECHA de la linea 1 (columnas 9-15), igual que teclado C/D.
        // Consistencia: el usuario ve la luz en la misma posicion independientemente
        // de si la ajusto por teclado o por serial.
        lcd_goto(1, 9);
        lcd_string("L:");
        lcd_int(dimmer_get()); // leer el nivel que efectivamente quedo (puede haber sido saturado)
        lcd_string("/10");

        usart_enviar_string("OK:LUZ,"); // respuesta con el formato "OK:LUZ,N"
        usart_enviar_int(dimmer_get());  // enviar el nivel aplicado realmente
        usart_enviar_newline();

    // ── GARAJE:ABRIR ─────────────────────────────────────────────────────────
    // strncmp(..., "GARAJE:ABRIR", 12): verificar el prefijo de 12 caracteres.
    } else if (strncmp(comando, "GARAJE:ABRIR", 12) == 0) {

        // Informar ANTES de iniciar el movimiento: garaje_abrir() es BLOQUEANTE
        // (~2.5 segundos). El usuario debe saber que el comando fue aceptado
        // aunque tarde en completarse.
        lcd_goto(1, 0);
        lcd_string("GARAJE ABRIENDO "); // 16 chars — ocupa toda la linea 1 temporalmente
        usart_enviar_string("OK:GARAJE_ABRIENDO");
        usart_enviar_newline();

        // Ejecutar el movimiento: gira MOTOR_PASOS_GARAJE pasos en sentido horario.
        // BLOQUEANTE: el loop se detiene aqui hasta terminar. Las ISR de alarma
        // (INT2-INT5) SI se ejecutan durante el _delay_ms() interno de motor_pasos().
        garaje_abrir();

        // Estado final en mitad derecha (cols 9-15) para que temp no lo pise
        lcd_goto(1, 0);
        lcd_string("         ABIERTO"); // 9 espacios + 7 chars en cols 9-15
        usart_enviar_string("OK:GARAJE_ABIERTO");
        usart_enviar_newline();

    // ── GARAJE:CERRAR ────────────────────────────────────────────────────────
    // strncmp(..., "GARAJE:CERRAR", 13): prefijo de 13 caracteres.
    } else if (strncmp(comando, "GARAJE:CERRAR", 13) == 0) {

        lcd_goto(1, 0);
        lcd_string("GARAJE CERRANDO "); // informar antes del movimiento bloqueante
        usart_enviar_string("OK:GARAJE_CERRANDO");
        usart_enviar_newline();

        // Gira MOTOR_PASOS_GARAJE pasos en sentido antihorario.
        // Misma logica bloqueante que garaje_abrir().
        garaje_cerrar();

        lcd_goto(1, 0);
        lcd_string("         CERRADO"); // 9 espacios + 7 chars en cols 9-15
        usart_enviar_string("OK:GARAJE_CERRADO");
        usart_enviar_newline();

     } else if (strncmp(comando, "HORNO:", 6) == 0) {

        // Formato: HORNO:<temp>,<segundos>  ej. "HORNO:180,5" -> 5 segundos
        // comando+6 apunta a "180,25"
        char* args = comando + 6;

        uint8_t temp = texto_a_numero(args);

        // Buscar la coma para saltar al segundo argumento (minutos)
        char* coma = strchr(args, ',');
        uint8_t minutos = 0;
        if (coma != NULL) {
            minutos = texto_a_numero(coma + 1);
        }

        horno_encender(temp, minutos);

        lcd_goto(0, 0); lcd_string("HORNO:          "); // plantilla limpia 16 chars
        lcd_goto(0, 6); lcd_int(temp); lcd_char('C');   // temperatura en col 6
        lcd_goto(1, 9); lcd_string("       ");          // limpiar mitad derecha fila 1
        lcd_goto(1, 9); lcd_int(minutos); lcd_string("s"); // segundos en cols 9+

        usart_enviar_string("OK:HORNO,");
        usart_enviar_int(temp);
        usart_enviar_string(",");
        usart_enviar_int(minutos);
        usart_enviar_newline();

    } else if (strncmp(comando, "SONIDO:OFF", 10) == 0) {

        sonido_apagar();
        lcd_goto(0, 0); lcd_string("SONIDO: OFF     ");
        usart_enviar_string("OK:SONIDO_OFF");
        usart_enviar_newline();

    } else if (strncmp(comando, "SONIDO:ON", 9) == 0) {

        // "SONIDO:ON"    -> encender con el ultimo volumen (o 5 si es la primera vez)
        // "SONIDO:ON,7"  -> encender al nivel 7 (0-10)
        uint8_t nivel = (sonido_volumen() > 0) ? sonido_volumen() : 5;
        char* coma = strchr(comando + 9, ',');
        if (coma != NULL) nivel = texto_a_numero(coma + 1);
        sonido_encender(nivel);

        lcd_goto(0, 0); lcd_string("SONIDO: ON      ");
        lcd_goto(1, 9); lcd_string("       ");           // limpiar mitad derecha
        lcd_goto(1, 9); lcd_string("V:"); lcd_int(sonido_volumen()); lcd_string("/10");

        usart_enviar_string("OK:SONIDO_ON,");
        usart_enviar_int(sonido_volumen());
        usart_enviar_newline();

    } else if (strncmp(comando, "VOL:", 4) == 0) {

        // VOL:+  -> sube 1 paso   VOL:-  -> baja 1 paso   VOL:N -> fija nivel N
        // Solo funciona si el equipo de sonido esta encendido
        if (sonido_estado() != SONIDO_ENCENDIDO) {
            usart_enviar_string("ERROR:SONIDO_APAGADO");
            usart_enviar_newline();
        } else {
            char* arg = comando + 4;
            uint8_t nuevo;
            if (*arg == '+') {
                nuevo = sonido_volumen() + 1;
            } else if (*arg == '-') {
                int8_t v = (int8_t)sonido_volumen() - 1;
                nuevo = (v < 0) ? 0 : (uint8_t)v;
            } else {
                nuevo = texto_a_numero(arg);
            }
            sonido_set_volumen(nuevo);

            lcd_goto(1, 9); lcd_string("       ");
            lcd_goto(1, 9); lcd_string("V:"); lcd_int(sonido_volumen()); lcd_string("/10");

            usart_enviar_string("OK:VOL,");
            usart_enviar_int(sonido_volumen());
            usart_enviar_newline();
        }

    } else if (strncmp(comando, "MERCADO:ADD,", 12) == 0) {

        // Formato: MERCADO:ADD,<nombre>,<cantidad>  ej. "MERCADO:ADD,leche,2"
        char* args = comando + 12;

        char* coma = strchr(args, ',');
        if (coma != NULL) {

            // Separar nombre y cantidad cortando el string en la coma
            *coma = '\0';                 // terminar el nombre justo en la coma
            uint8_t cantidad = texto_a_numero(coma + 1);

            uint8_t r = mercado_agregar(args, cantidad);
            if (r == 0) {
                usart_enviar_string("ERROR:MERCADO_LLENO");
            } else {
                // r==2: ya existia y se reemplazo la cantidad; r==1: nuevo
                usart_enviar_string((r == 2) ? "OK:MERCADO_ACTUALIZADO" : "OK:MERCADO_AGREGADO");
            }
        } else {
            usart_enviar_string("ERROR:FORMATO_MERCADO");
        }
        usart_enviar_newline();

    } else if (strncmp(comando, "MERCADO:DEL,", 12) == 0) {

        char* nombre = comando + 12;

        if (mercado_eliminar(nombre)) {
            usart_enviar_string("OK:MERCADO_ELIMINADO");
        } else {
            usart_enviar_string("ERROR:MERCADO_NO_ENCONTRADO");
        }
        usart_enviar_newline();

     } else if (strncmp(comando, "MERCADO:LIST", 12) == 0) {

        usart_enviar_string("--- LISTA DE MERCADO ---");
        usart_enviar_newline();
        mercado_listar();   // esta funcion ya envia cada item + newline internamente

    // ── FASE 4: ENROL:ADULTO,codigo ──────────────────────────────────────
    } else if (strncmp(comando, "ENROL:ADULTO,", 13) == 0) {

        char* codigo = comando + 13;
        if (alarma_verificar_codigo(codigo)) {
            rfid_entrar_modo_enrol(0, 0);
            usart_enviar_string("OK:ENROLANDO_ADULTO");
        } else {
            reportar_codigo_incorrecto();
        }
        usart_enviar_newline();

    // ── FASE 4: ENROL:HIJO,n,codigo ──────────────────────────────────────
    } else if (strncmp(comando, "ENROL:HIJO,", 11) == 0) {

        // Formato: ENROL:HIJO,<cupos>,<codigo>
        char* resto = comando + 11;
        uint8_t cupos = texto_a_numero(resto);
        char* coma = strchr(resto, ',');
        char* codigo = (coma != NULL) ? coma + 1 : resto;

        if (alarma_verificar_codigo(codigo)) {
            rfid_entrar_modo_enrol(1, cupos);
            usart_enviar_string("OK:ENROLANDO_HIJO,");
            usart_enviar_int(cupos);
        } else {
            reportar_codigo_incorrecto();
        }
        usart_enviar_newline();

    // ── FASE 4: BORRAR,codigo ────────────────────────────────────────────
    } else if (strncmp(comando, "BORRAR,", 7) == 0) {

        char* codigo = comando + 7;
        if (alarma_verificar_codigo(codigo)) {
            rfid_entrar_modo_borrar();
            usart_enviar_string("OK:BORRANDO");
        } else {
            reportar_codigo_incorrecto();
        }
        usart_enviar_newline();

    // ── FASE 4: ACCESOS:n,codigo ─────────────────────────────────────────
    } else if (strncmp(comando, "ACCESOS:", 8) == 0) {

        // Formato: ACCESOS:<n>,<codigo>
        char* resto = comando + 8;
        uint8_t cantidad = texto_a_numero(resto);
        char* coma = strchr(resto, ',');
        char* codigo = (coma != NULL) ? coma + 1 : resto;

        if (alarma_verificar_codigo(codigo)) {
            rfid_entrar_modo_recarga(cantidad);
            usart_enviar_string("OK:RECARGANDO,");
            usart_enviar_int(cantidad);
        } else {
            reportar_codigo_incorrecto();
        }
        usart_enviar_newline();

    // ── COMANDO DESCONOCIDO ───────────────────────────────────────────────────
    } else {
        // Ningun prefijo conocido coincidio. Reportar el error sin tomar accion.
        // El usuario puede ver el mensaje en la terminal y corregir el comando.
        usart_enviar_string("ERROR:COMANDO_DESCONOCIDO");
        usart_enviar_newline();
    }
}


// ─────────────────────────────────────────────────────────────────────────────
// producto_por_codigo(cod)
// Catalogo fijo de productos del mercado para el entrenador (sin terminal no se
// pueden teclear nombres). Cada producto tiene un codigo numerico de 2 digitos.
// Retorna el nombre (string en Flash) o NULL si el codigo no existe.
// Para agregar productos, ampliar este switch y la tabla en COMANDOS_TECLADO.md.
// ─────────────────────────────────────────────────────────────────────────────
static const char* producto_por_codigo(uint8_t cod) {
    switch (cod) {
        case 1:  return "leche";
        case 2:  return "pan";
        case 3:  return "huevos";
        case 4:  return "arroz";
        case 5:  return "aceite";
        case 6:  return "azucar";
        case 7:  return "cafe";
        case 8:  return "sal";
        case 9:  return "pasta";
        case 10: return "agua";
        default: return NULL; // codigo no catalogado
    }
}


// ─────────────────────────────────────────────────────────────────────────────
// (mostrar_mercado_lcd ELIMINADA) — el listado del mercado ahora va por la
// terminal serial de forma NO bloqueante (ver case 59 en ejecutar_comando_teclado).
// Se quito porque su version con _delay_ms congelaba todo el sistema mientras
// listaba (no respondian teclado ni alarmas) — justo el bug reportado.
// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
// ejecutar_comando_teclado(cmd)
// Interpreta y ejecuta un comando capturado en modo teclado. Formato:
//   <FF><param1>[A<param2>]   donde FF = codigo de funcion de 2 digitos.
// La tecla 'A' separa el primer parametro del segundo (horno y mercado-add).
// Reutiliza las MISMAS funciones de modulo que el parser serial.
// Ver la tabla completa de combinaciones en COMANDOS_TECLADO.md.
// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
// FUNCIONES DEL LETRERO DESLIZANTE (marquee) DE LA LINEA 0
// ─────────────────────────────────────────────────────────────────────────────

// Repinta la linea 0 con el estado ACTUAL de la alarma. Se llama cuando el
// letrero termina, para devolver la pantalla a su aspecto normal.
static void repintar_alarma_lcd() {
    lcd_goto(0, 0);
    if (alarma_estado() == ALARMA_ARMADA) {
        lcd_string("ALARMA: ARMADA  ");
    } else if (alarma_estado() == ALARMA_DISPARADA) {
        if (alarma_tipo_disparo() == ALARMA_TIPO_INCENDIO)    lcd_string("!! INCENDIO !!  ");
        else if (alarma_tipo_disparo() == ALARMA_TIPO_ACCESO) lcd_string("!! INTRUSION !! ");
        else                                                  lcd_string("!! INTRUSO !!   ");
    } else {
        lcd_string("ALARMA: DESACTIV");
    }
}

// Dibuja en la linea 0 una "ventana" de 16 caracteres del mensaje, empezando en
// marquee_off. Lo que cae fuera del mensaje se rellena con espacios para borrar
// el frame anterior.
static void marquee_render() {
    lcd_goto(0, 0);
    for (uint8_t i = 0; i < 16; i++) {
        uint8_t idx = marquee_off + i;
        lcd_char(idx < marquee_len ? marquee_buf[idx] : ' ');
    }
}

// Inicia la presentacion de 'msg'. Copia el texto a un buffer propio. Si cabe
// (<=16) lo deja FIJO; si es mas largo, activa el deslizamiento.
static void marquee_mostrar(const char* msg) {
    uint8_t n = 0;
    while (msg[n] && n < sizeof(marquee_buf) - 1) {
        marquee_buf[n] = msg[n];
        n++;
    }
    marquee_buf[n] = '\0';
    marquee_len  = n;
    marquee_off  = 0;
    marquee_tick = 0;
    marquee_render();                              // primer frame inmediato
    marquee_activo = (marquee_len > 16) ? 1 : 0;   // solo desliza si NO cabe
}

// Avanza el letrero un paso cuando corresponde. Llamar en CADA vuelta del loop.
// No hace nada si no hay letrero activo (caso comun), asi que es muy barato.
static void marquee_actualizar() {
    if (!marquee_activo) return;
    marquee_tick++;
    if (marquee_tick < MARQUEE_VELOCIDAD) return;  // aun no toca mover
    marquee_tick = 0;
    marquee_off++;
    if (marquee_off > marquee_len) {               // el mensaje ya salio por la izquierda
        marquee_activo = 0;
        repintar_alarma_lcd();                     // devolver la linea 0 a su estado normal
        return;
    }
    marquee_render();
}


static void ejecutar_comando_teclado(char* cmd) {

    // Medir el largo del comando tecleado
    uint8_t n = 0;
    while (cmd[n]) n++;

    // Validar que haya al menos el codigo de funcion de 2 digitos
    if (n < 2 || cmd[0] < '0' || cmd[0] > '9' || cmd[1] < '0' || cmd[1] > '9') {
        lcd_goto(0, 0);
        lcd_string("CMD INVALIDO    ");
        usart_enviar_string("ERROR:CMD_TECLADO");
        usart_enviar_newline();
        return;
    }

    // Codigo de funcion (2 primeros digitos) y puntero a los parametros
    uint8_t func = (cmd[0] - '0') * 10 + (cmd[1] - '0');
    char*   args = cmd + 2;

    // param1 = primer numero; param2 = numero despues de la 'A' (si existe)
    uint8_t p1 = texto_a_numero(args);
    char*   sep = strchr(args, 'A');
    uint8_t p2 = (sep != NULL) ? texto_a_numero(sep + 1) : 0;

    switch (func) {

        // ── 1x GARAJE ────────────────────────────────────────────────────────
        case 11: // abrir
            lcd_goto(1, 0); lcd_string("GARAJE ABRIENDO ");
            usart_enviar_string("OK:GARAJE_ABRIENDO"); usart_enviar_newline();
            garaje_abrir();
            lcd_goto(1, 0); lcd_string("         ABIERTO"); // cols 9-15
            usart_enviar_string("OK:GARAJE_ABIERTO"); usart_enviar_newline();
            break;
        case 10: // cerrar
            lcd_goto(1, 0); lcd_string("GARAJE CERRANDO ");
            usart_enviar_string("OK:GARAJE_CERRANDO"); usart_enviar_newline();
            garaje_cerrar();
            lcd_goto(1, 0); lcd_string("         CERRADO"); // cols 9-15
            usart_enviar_string("OK:GARAJE_CERRADO"); usart_enviar_newline();
            break;

        // ── 2x SONIDO ────────────────────────────────────────────────────────
        case 21: // encender con nivel (p1 = 0-10)
            sonido_encender(p1);
            lcd_goto(0, 0); lcd_string("SONIDO: ON      ");
            lcd_goto(1, 9); lcd_string("       ");
            lcd_goto(1, 9); lcd_string("V:"); lcd_int(sonido_volumen()); lcd_string("/10");
            usart_enviar_string("OK:SONIDO_ON,");
            usart_enviar_int(sonido_volumen()); usart_enviar_newline();
            break;
        case 20: // apagar
            sonido_apagar();
            lcd_goto(0, 0); lcd_string("SONIDO: OFF     ");
            usart_enviar_string("OK:SONIDO_OFF"); usart_enviar_newline();
            break;
        case 22: { // vol+ (sube 1 paso) — solo si el equipo esta encendido
            if (sonido_estado() != SONIDO_ENCENDIDO) {
                usart_enviar_string("ERROR:SONIDO_APAGADO"); usart_enviar_newline();
            } else {
                uint8_t vp = sonido_volumen() + 1;
                sonido_set_volumen(vp);
                lcd_goto(1, 9); lcd_string("       ");
                lcd_goto(1, 9); lcd_string("V:"); lcd_int(sonido_volumen()); lcd_string("/10");
                usart_enviar_string("OK:VOL,"); usart_enviar_int(sonido_volumen()); usart_enviar_newline();
            }
            break;
        }
        case 23: { // vol- (baja 1 paso) — solo si el equipo esta encendido
            if (sonido_estado() != SONIDO_ENCENDIDO) {
                usart_enviar_string("ERROR:SONIDO_APAGADO"); usart_enviar_newline();
            } else {
                int8_t vm = (int8_t)sonido_volumen() - 1;
                sonido_set_volumen((vm < 0) ? 0 : (uint8_t)vm);
                lcd_goto(1, 9); lcd_string("       ");
                lcd_goto(1, 9); lcd_string("V:"); lcd_int(sonido_volumen()); lcd_string("/10");
                usart_enviar_string("OK:VOL,"); usart_enviar_int(sonido_volumen()); usart_enviar_newline();
            }
            break;
        }

        // ── 3x HORNO ─────────────────────────────────────────────────────────
        case 31: // encender (p1 = temp, p2 = minutos)
            horno_encender(p1, p2);
            lcd_goto(0, 0); lcd_string("HORNO:          "); // plantilla limpia
            lcd_goto(0, 6); lcd_int(p1); lcd_char('C');     // temperatura en col 6
            lcd_goto(1, 9); lcd_string("       ");          // limpiar mitad derecha fila 1
            lcd_goto(1, 9); lcd_int(p2); lcd_string("s"); // segundos en cols 9+
            usart_enviar_string("OK:HORNO,");
            usart_enviar_int(p1); usart_enviar_string(",");
            usart_enviar_int(p2); usart_enviar_newline();
            break;
        case 30: // apagar
            horno_apagar();
            lcd_goto(0, 0); lcd_string("HORNO: OFF      ");
            usart_enviar_string("OK:HORNO_OFF"); usart_enviar_newline();
            break;

        // ── 4x LUZ (dimmer por valor directo) ────────────────────────────────
        case 41: // fijar nivel (p1 = 0-10)
            dimmer_set(p1);
            lcd_goto(1, 9); lcd_string("       ");
            lcd_goto(1, 9); lcd_string("L:"); lcd_int(dimmer_get()); lcd_string("/10");
            usart_enviar_string("OK:LUZ,");
            usart_enviar_int(dimmer_get()); usart_enviar_newline();
            break;

        // ── 5x MERCADO (catalogo por codigo) ─────────────────────────────────
        case 51: { // agregar (p1 = codigo producto, p2 = cantidad)
            const char* nom = producto_por_codigo(p1);
            if (nom == NULL) {
                lcd_goto(0, 0); lcd_string("PROD DESCONOCIDO");
                usart_enviar_string("ERROR:PRODUCTO_DESCONOCIDO");
            } else {
                uint8_t r = mercado_agregar(nom, p2);
                if (r == 0) {
                    // Lista llena. Ya NO existe el caso "duplicado": los
                    // repetidos se actualizan, no se rechazan.
                    lcd_goto(0, 0); lcd_string("MERCADO LLENO   ");
                    usart_enviar_string("ERROR:MERCADO_LLENO");
                } else {
                    // r==1 producto nuevo; r==2 ya existia y se reemplazo la cantidad.
                    // Letrero "<producto> AGREGADO/ACTUALIZADO" (fijo si cabe, desliza si no)
                    char msg[40];
                    strcpy(msg, nom);
                    strcat(msg, (r == 2) ? " ACTUALIZADO" : " AGREGADO");
                    marquee_mostrar(msg);
                    usart_enviar_string((r == 2) ? "OK:MERCADO_ACTUALIZADO" : "OK:MERCADO_AGREGADO");
                }
            }
            usart_enviar_newline();
            break;
        }
        case 50: { // eliminar (p1 = codigo producto)
            const char* nom = producto_por_codigo(p1);
            if (nom == NULL) {
                lcd_goto(0, 0); lcd_string("PROD DESCONOCIDO");
                usart_enviar_string("ERROR:PRODUCTO_DESCONOCIDO");
            } else if (mercado_eliminar(nom)) {
                // Letrero "<producto> ELIMINADO" (fijo si cabe, deslizante si no)
                char msg[40];
                strcpy(msg, nom);
                strcat(msg, " ELIMINADO");
                marquee_mostrar(msg);
                usart_enviar_string("OK:MERCADO_ELIMINADO");
            } else {
                lcd_goto(0, 0); lcd_string("MERC NO EXISTE  ");
                usart_enviar_string("ERROR:MERCADO_NO_ENCONTRADO");
            }
            usart_enviar_newline();
            break;
        }
        case 59: // listar el mercado por la TERMINAL (serial), NO bloqueante.
            // En el LCD solo se muestra el conteo (rapido, sin _delay); la lista
            // completa va a la Virtual Terminal por USART (mercado_listar()).
            lcd_goto(0, 0); lcd_string("MERCADO: ");
            lcd_int(mercado_contar()); lcd_string(" item ");
            usart_enviar_string("--- LISTA DE MERCADO ---"); usart_enviar_newline();
            mercado_listar();
            break;

        // ── 6x RFID/ACCESO (Fase 4, requiere codigo alarma) ──────────────
        case 61: // enrolar adulto: *61<codigo>#
            if (alarma_verificar_codigo(args)) {
                rfid_entrar_modo_enrol(0, 0);
                lcd_goto(0, 0); lcd_string("ENROL ADULTO    ");
                usart_enviar_string("OK:ENROLANDO_ADULTO");
            } else {
                reportar_codigo_incorrecto();
            }
            usart_enviar_newline();
            break;

        case 62: { // enrolar hijo: *62<cupos>A<codigo>#
            // args = "<cupos>A<codigo>", p1 = cupos; extraer codigo tras 'A'
            char* sep62 = strchr(args, 'A');
            char* codigo62 = (sep62 != NULL) ? sep62 + 1 : args;
            if (alarma_verificar_codigo(codigo62)) {
                rfid_entrar_modo_enrol(1, p1);
                lcd_goto(0, 0); lcd_string("ENROL HIJO      ");
                usart_enviar_string("OK:ENROLANDO_HIJO,");
                usart_enviar_int(p1);
            } else {
                reportar_codigo_incorrecto();
            }
            usart_enviar_newline();
            break;
        }

        case 60: // borrar tarjeta: *60<codigo>#
            if (alarma_verificar_codigo(args)) {
                rfid_entrar_modo_borrar();
                lcd_goto(0, 0); lcd_string("BORRAR TARJETA  ");
                usart_enviar_string("OK:BORRANDO");
            } else {
                reportar_codigo_incorrecto();
            }
            usart_enviar_newline();
            break;

        case 63: { // recargar cupos de un hijo: *63<cantidad>A<codigo># y luego pasar la tarjeta
            // args = "<cantidad>A<codigo>", p1 = cantidad; codigo tras la 'A'
            char* sep63 = strchr(args, 'A');
            char* codigo63 = (sep63 != NULL) ? sep63 + 1 : args;
            if (alarma_verificar_codigo(codigo63)) {
                rfid_entrar_modo_recarga(p1);
                lcd_goto(0, 0); lcd_string("RECARGAR HIJO   ");
                usart_enviar_string("OK:RECARGANDO,");
                usart_enviar_int(p1);
            } else {
                reportar_codigo_incorrecto();
            }
            usart_enviar_newline();
            break;
        }

        // ── 9x ALARMA (armar/desarmar con codigo) ────────────────────────────
        case 91: // armar (args = codigo de 4 digitos, ej. *91 1234 #)
            if (alarma_verificar_codigo(args)) {
                alarma_armar();
                alarma_reset_intentos(); // codigo correcto: limpiar contador anti-intrusos
                lcd_goto(0, 0); lcd_string("ALARMA: ARMADA  ");
                usart_enviar_string("OK:ARMADA");
            } else {
                reportar_codigo_incorrecto(); // cuenta el fallo (puede disparar intruso)
            }
            usart_enviar_newline();
            break;
        case 90: // desarmar (args = codigo de 4 digitos)
            if (alarma_verificar_codigo(args)) {
                alarma_desarmar();
                alarma_reset_intentos(); // codigo correcto: limpiar contador anti-intrusos
                lcd_goto(0, 0); lcd_string("ALARMA: DESACTIV");
                usart_enviar_string("OK:DESACTIVADA");
            } else {
                reportar_codigo_incorrecto(); // cuenta el fallo (puede disparar intruso)
            }
            usart_enviar_newline();
            break;

        // ── CODIGO NO RECONOCIDO ─────────────────────────────────────────────
        default:
            lcd_goto(0, 0); lcd_string("CMD DESCONOCIDO ");
            usart_enviar_string("ERROR:CMD_TECLADO");
            usart_enviar_newline();
            break;
    }
}


// ─────────────────────────────────────────────────────────────────────────────
// setup()
// Funcion especial de Arduino — se ejecuta UNA sola vez al encender o resetear.
// Inicializa todos los modulos en el orden correcto y deja el sistema listo.
// REGLA: sei() SIEMPRE al final de los init(), nunca antes.
// ─────────────────────────────────────────────────────────────────────────────
void setup() {

    lcd_init();       // Fase 1: configura Puerto A como salida y ejecuta la secuencia HD44780
    teclado_init();   // Fase 2: configura Puerto L (cols), Puerto C (filas) y Timer2 CTC
    usart_init();     // Fase 3: configura USART0 9600 bps 8N1 y habilita ISR de recepcion
    alarma_init();    // Fase 5: configura LEDs PB6/PB7, EICRA/EICRB, habilita INT4/INT5
    adc_init();       // ADC compartido (AVCC, prescaler 128) — temperatura en ADC9 (A9)
    motor_init();     // Fase 6a: SERVO del garaje, Timer4 PWM 50Hz en OC4A (PH3/D6)
    temp_init();      // Fase 6b: pines calefactor PK3 (A11) y ventilador PK4 (A12)
    dimmer_init();    // Fase 6c: configura Timer1 Fast PWM en OC1A (PB5/D11)
    horno_init();      // Fase 7a: LED horno en PH5 (pin 8)
    sonido_init();     // Fase 7b: Timer3 PWM volumen (PE3/D5)->RC + LED PH6 (D9); volumen por comando serial
    mercado_init();     // Fase 7c: lista de mercado en EEPROM (sin hardware)

    // Fase 4: RFID y control de acceso
    // RST del RC522 en pin 4 (PG5): HIGH = activo, LOW = power-down (no responde a SPI)
    DDRG  |= (1 << PG5);
    PORTG |= (1 << PG5);
    _delay_ms(50);      // esperar que oscilador del RC522 estabilice (libreria usa 50ms)
    spi_init();         // SPI maestro (PB0-PB3) para RC522
    eeprom_init();      // gestor de UIDs en EEPROM (0x000-0x09F)
    rfid_init();        // modulo RC522 por SPI
    acceso_init();      // iman puerta principal (PG0) + garaje (servo)

    // Pulsadores selectores de lugar: D40 (PG1) y D39 (PG2) como ENTRADAS,
    // activo en ALTO (el entrenador los maneja por debajo). No tocan PG0 (iman).
    DDRG  &= ~((1 << PG1) | (1 << PG2));   // entradas
    PORTG &= ~((1 << PG1) | (1 << PG2));   // sin pull-up interno

    // sei(): habilita las interrupciones globales poniendo el bit I del SREG en 1.
    // DEBE ir DESPUES de todos los init() porque:
    //   - teclado_init() configuro Timer2 (ya listo para disparar ISR)
    //   - alarma_init() configuro EIMSK con INT4/INT5 habilitados
    //   - usart_init() configuro RXCIE0 (ISR de recepcion serial habilitada)
    // Si sei() se llamara ANTES, cualquiera de esas ISR podria dispararse mientras
    // otro modulo aun esta configurando sus registros → comportamiento indefinido.
    sei();

    lcd_clear(); // borrar pantalla y mover cursor a (0,0) — limpia cualquier basura de inicio

    lcd_goto(0, 0);              // posicionar cursor en linea 0, columna 0
    lcd_string("ALARMA: DESACTIV"); // estado inicial de la alarma en la linea dedicada (16 chars)

    lcd_goto(1, 0);              // posicionar cursor en linea 1, columna 0
    lcd_string("T:--C    L:0/10 "); // "L" en col 9 (igual que lcd_goto(1,9) en updates de luz)

    usart_enviar_newline(); // linea en blanco para separar el encabezado en la terminal
    // Enviar el menu de comandos disponibles a la terminal al encender
    usart_enviar_string("Comandos: ARM:xxxx / DISARM:xxxx / LUZ:0-10 / GARAJE:ABRIR / GARAJE:CERRAR");
    usart_enviar_newline();
    usart_enviar_string("ENROL:ADULTO / ENROL:HIJO,N / BORRAR / ACCESOS:N");
    usart_enviar_newline();
    usart_enviar_string("HORNO:temp,min / SONIDO:ON[,0-10] / SONIDO:OFF / VOL:N / VOL:+ / VOL:- / MERCADO:ADD,nom,cant / MERCADO:DEL,nom / MERCADO:LIST");
    usart_enviar_newline();
    pos_codigo = 0; // asegurar que el buffer del codigo empiece vacio (redundante pero explicito)
}


// ─────────────────────────────────────────────────────────────────────────────
// loop()
// Funcion especial de Arduino — se llama repetidamente de forma infinita
// despues de setup(). Cada iteracion ("vuelta del loop") revisa todos los
// subsistemas y atiende los eventos pendientes.
//
// ORDEN DE PRIORIDAD (de mayor a menor urgencia):
//   1. Alarmas    — seguridad critica, siempre primero
//   2. Teclado    — interaccion del usuario fisica
//   3. Serial     — comandos remotos
//   4. Temperatura — actualizacion periodica, menos urgente
// ─────────────────────────────────────────────────────────────────────────────
void loop() {

    // ── 1. ALARMAS (Fase 5) ──────────────────────────────────────────────────
    // alarma_actualizar(): revisa si las ISR (INT2-INT5) dejaron alguna bandera activa.
    // Retorna 1 si hubo un evento NUEVO que reportar en esta vuelta, 0 si no.
    // El if consume el evento: la proxima vuelta del loop no volvera a reportarlo
    // a menos que llegue un nuevo disparo.
    if (alarma_actualizar()) {

        // alarma_tipo_disparo(): retorna que tipo de evento causo el disparo.
        //   ALARMA_TIPO_INCENDIO (1): alguno de los sensores SW3/SW4 activo INT4/INT5.
        //   ALARMA_TIPO_ACCESO   (2): alguno de los sensores SW1/SW2 activo INT2/INT3
        //                             y la alarma de acceso estaba ARMADA.
        uint8_t tipo = alarma_tipo_disparo();

        // Un evento de alarma tiene prioridad sobre la linea 0: cancelar el
        // letrero del mercado para que no vuelva a pisar el mensaje de alarma.
        marquee_activo = 0;

        // Responder segun el tipo de evento detectado
        if (tipo == ALARMA_TIPO_INCENDIO) {
            lcd_goto(0, 0);              // linea 0: exclusiva del estado de alarma
            lcd_string("!! INCENDIO !!  "); // 16 chars — exclamaciones llaman la atencion
            usart_enviar_string("ALARMA:INCENDIO"); // alerta a la terminal remota
            usart_enviar_newline();

        } else if (tipo == ALARMA_TIPO_ACCESO) {
            lcd_goto(0, 0);
            lcd_string("!! INTRUSION !! "); // 16 chars
            usart_enviar_string("ALARMA:ACCESO");
            usart_enviar_newline();
        }
        // Si tipo fuera ALARMA_TIPO_NINGUNO (0), alarma_actualizar() no habria retornado 1
        // (eso se garantiza dentro de alarma.cpp), asi que no hay caso else aqui.
    }

    // ── 1b. RFID (Fase 4) ─────────────────────────────────────────────────────
    rfid_verificar();

    // ── 1c. PULSADORES SELECTORES DE LUGAR ───────────────────────────────────
    // D40 (PG1) = izquierda (retrocede), D39 (PG2) = derecha (avanza).
    // PULL-UP: en reposo se lee 1; al pulsar (a GND) se lee 0. Antirrebote real.
    {
        uint8_t izq = (PING & (1 << PG1)) ? 1 : 0;  // 1 = presionado (activo en alto)
        uint8_t der = (PING & (1 << PG2)) ? 1 : 0;

        if (millis_sistema() - btn_ultimo_ms >= BTN_DEBOUNCE_MS) {
            if (btn_izq_prev == 0 && izq == 1) {            // flanco de pulsacion izquierda
                lugar_actual = (lugar_actual + LUGAR_TOTAL - 1) % LUGAR_TOTAL;
                mostrar_lugar();
                btn_ultimo_ms = millis_sistema();
            } else if (btn_der_prev == 0 && der == 1) {     // flanco de pulsacion derecha
                lugar_actual = (lugar_actual + 1) % LUGAR_TOTAL;
                mostrar_lugar();
                btn_ultimo_ms = millis_sistema();
            }
        }
        btn_izq_prev = izq;
        btn_der_prev = der;
    }

    // ── 1d. RETARDO DE DESARME (entrada por puerta/garaje) ────────────────────
    // Tras conceder acceso por RFID en puerta/garaje (con alarma armada), hay
    // 10s/15s para desarmar con el codigo. Si se desarma, alarma_estado() deja
    // de ser ARMADA y el retardo se cancela solo. Si vence, se dispara intruso.
    if (retardo_activo) {
        if (alarma_estado() != ALARMA_ARMADA) {
            retardo_activo = 0;   // se desarmo (o ya se disparo): cancelar
        } else {
            uint32_t transcurrido = millis_sistema() - retardo_inicio;
            if (transcurrido >= retardo_total_ms) {
                // Vencio el tiempo sin desarmar -> intrusion
                retardo_activo = 0;
                alarma_disparar_intruso();
                lcd_goto(0, 0); lcd_string("!! INTRUSO !!   ");
                usart_enviar_string("ALARMA:INTRUSO"); usart_enviar_newline();
            } else {
                // Cuenta regresiva en la linea 0 (solo al cambiar de segundo)
                uint8_t seg = (uint8_t)((retardo_total_ms - transcurrido + 999) / 1000);
                if (seg != retardo_ult_seg) {
                    retardo_ult_seg = seg;
                    lcd_goto(0, 0); lcd_string("DESARMA EN: ");
                    lcd_int(seg); lcd_string("s ");
                }
            }
        }
    }

    // ── 2. TECLADO (Fase 2 + logica de alarma/dimmer) ────────────────────────
    // teclado_hay(): retorna 1 si la ISR del Timer2 deposito una tecla nueva.
    //   Se verifica ANTES de leer para no llamar teclado_leer() sin tecla disponible
    //   (que retornaria '\0' y podria interpretarse como una pulsacion vacia).
    if (teclado_hay()) {

        // teclado_leer(): retorna el caracter de la tecla y CONSUME la bandera.
        //   Siguiente llamada a teclado_hay() retornara 0 hasta nueva pulsacion.
        char tecla = teclado_leer();

        // Delegar la interpretacion de la tecla a la funcion dedicada.
        // Dentro de procesar_tecla_alarma() se reinicia ultima_interaccion = 0.
        procesar_tecla_alarma(tecla);
    }

    // ── 3. COMANDOS SERIALES (Fase 3) ────────────────────────────────────────
    // usart_hay_linea(): retorna 1 si el buffer circular de RX tiene un '\n' o '\r'.
    //   Una linea completa indica que el usuario termino de escribir el comando.
    if (usart_hay_linea()) {

        char comando[32];

        // Si la linea es vacia (n=0) ignorarla: puede ser el '\n' sobrante
        // del par \r\n que la terminal envia al presionar Enter.
        if (usart_leer_linea(comando) > 0) {
            a_mayusculas(comando);
            procesar_comando_serial(comando);
        }
    }

    // ── 3b. VOLUMEN DEL EQUIPO DE SONIDO (Fase 7) ────────────────────────────
    // Si el equipo esta encendido, lee el potenciometro (A13/PK5) y ajusta el
    // volumen en tiempo real. No bloquea: solo lee el ADC y escribe el PWM.
    sonido_actualizar();

    // ── 3c. LETRERO DESLIZANTE DEL MERCADO ───────────────────────────────────
    // Avanza un paso del marquee cuando toca. No bloquea: si no hay letrero
    // activo retorna de inmediato.
    marquee_actualizar();

    // ── 4. TEMPERATURA Y TEMPORIZADO (Fase 6) ────────────────────────────────
    // Base de tiempo REAL (millis_sistema, Timer2) en vez de "vueltas del loop":
    // asi la temperatura y los actuadores responden cada INTERVALO_TEMP ms aunque
    // el RFID haga el loop lento/variable (antes llegaban tardisimo al LCD).
    if (millis_sistema() - ultima_lectura_temp >= INTERVALO_TEMP) {

        ultima_lectura_temp = millis_sistema(); // marcar el momento de esta lectura

        // CONDICION DEL LCD: solo actualizar la mitad izquierda de la linea 1
        // si el usuario lleva TIEMPO_MOSTRAR_INTERACCION ms sin interactuar.
        // IMPORTANTE: temp_controlar() (calefactor/ventilador) se ejecuta SIEMPRE
        // independientemente de esta condicion — la seguridad termica no se pausa
        // solo porque el usuario este digitando un codigo.
        if (millis_sistema() - ultima_interaccion >= TIEMPO_MOSTRAR_INTERACCION) {

            int8_t temp_actual = temp_celsius(); // leer temperatura del ADC (promedio de 8 muestras)

            // Escribir en columnas 0-8 de la linea 1 (mitad izquierda).
            // "T:25C   ": 7 caracteres. Los espacios finales borran un posible
            // valor anterior de 3 digitos si ahora el valor es de 1 digito (ej. "T:5C").
            lcd_goto(1, 0);       // posicionar en linea 1, columna 0
            lcd_string("T:");     // prefijo de temperatura
            lcd_int(temp_actual); // valor en °C: 1 o 2 digitos (rango 0-40°C en esta version)
            lcd_string("C   ");   // unidad + espacios para limpiar digitos sobrantes
        }

        // Evaluar umbrales con histeresis y actuar sobre calefactor (PK3) y ventilador (PK4).
        // Se llama despues del bloque LCD para que temp_celsius() ya fue llamado
        // (aunque temp_controlar() hace su propia llamada interna — es redundante
        //  pero claro: cada modulo es responsable de su propia lectura).
        temp_controlar();
    }
     // -- Horno (Fase 7a) --------------------------------------------------
    if (horno_actualizar()) {
        // El horno acaba de terminar su cuenta regresiva en esta vuelta
        lcd_goto(0, 0);
        lcd_string("HORNO: LISTO    ");
        usart_enviar_string("HORNO:FIN");
        usart_enviar_newline();
    }

    // ── 5. AUTO-RECUPERACION DEL LCD (anti-corrupcion por ruido) ─────────────
    // Si el ruido del entrenador (servo/RF/cables) desincroniza el LCD y queda
    // mostrando basura, esto lo re-sincroniza y repinta la pantalla estandar.
    // Solo corre EN REPOSO (sin interaccion reciente ni retardo de desarme), asi
    // no estorba a los mensajes de RFID/teclado/garaje.
    if (!retardo_activo &&
        (millis_sistema() - ultima_interaccion) >= TIEMPO_MOSTRAR_INTERACCION &&
        (millis_sistema() - ultima_resync_lcd)  >= RESYNC_LCD_MS) {

        ultima_resync_lcd = millis_sistema();

        lcd_resync();   // realinear el controlador (sin borrar -> sin parpadeo)

        // Repintar la pantalla estandar: linea 0 = alarma, linea 1 = temp + luz
        lcd_goto(0, 0);
        if      (alarma_estado() == ALARMA_ARMADA)    lcd_string("ALARMA: ARMADA  ");
        else if (alarma_estado() == ALARMA_DISPARADA) lcd_string("!! ALARMA !!    ");
        else                                          lcd_string("ALARMA: DESACTIV");

        lcd_goto(1, 0); lcd_string("T:"); lcd_int(temp_celsius()); lcd_string("C   ");
        lcd_goto(1, 9); lcd_string("L:"); lcd_int(dimmer_get());   lcd_string("/10");
    }
}

