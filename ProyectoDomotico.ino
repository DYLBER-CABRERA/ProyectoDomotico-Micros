// ProyectoDomotico.ino - Archivo principal del sistema domotico
// Fases activas: 1 (LCD) + 2 (Teclado) + 3 (USART) + 5 (Alarma) + 6 (Motor+Temp+Dimmer)
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

// Modulos activos del proyecto — cada header expone solo la API publica del modulo
#include "lcd.h"         // lcd_init, lcd_goto, lcd_string, lcd_int, lcd_char, lcd_clear
#include "teclado.h"     // teclado_init, teclado_hay, teclado_leer
#include "usart.h"       // usart_init, usart_enviar_*, usart_hay_linea, usart_leer_linea
#include "alarma.h"      // alarma_init/armar/desarmar/actualizar, ALARMA_TIPO_*, ALARMA_CODIGO_LEN
#include "motor.h"       // motor_init, garaje_abrir, garaje_cerrar
#include "temperatura.h" // temp_init, temp_celsius, temp_controlar
#include "dimmer.h"      // dimmer_init, dimmer_set, dimmer_get, DIMMER_NIVEL_MAX

// Fases siguientes — comentados hasta que se implemente cada modulo.
// Cuando una fase este lista: descomentar el include correspondiente,
// llamar a su _init() en setup() y su _actualizar() en loop().
// #include "spi_master.h"  // Fase 4: bus SPI para el modulo RC522 RFID
// #include "rfid.h"        // Fase 4: lectura de tarjetas RFID
// #include "eeprom_mgr.h"  // Fase 4: UIDs autorizados en EEPROM interna
// #include "acceso.h"      // Fase 4: logica de acceso por tarjeta
// #include "horno.h"       // Fase 7: control remoto del horno
// #include "sonido.h"      // Fase 7: control del equipo de sonido
// #include "mercado.h"     // Fase 7: lista de mercado


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

// Contador de iteraciones del loop() para temporizar la lectura de temperatura.
// Se incrementa 1 vez por vuelta del loop. Al superar INTERVALO_TEMP se hace
// la lectura del ADC y se evaluan los umbrales, luego se resetea a 0.
// uint16_t: 16 bits sin signo (0-65535), necesario porque INTERVALO_TEMP = 2000.
static uint16_t contador_temp = 0;

// Cantidad de ciclos del loop() entre lecturas consecutivas del sensor de temperatura.
// #define: sustitucion textual en tiempo de compilacion, NO ocupa RAM (SRAM).
// Con el loop corriendo a varios kHz, 2000 ciclos equivalen a aproximadamente
// 2-3 segundos de intervalo entre lecturas (depende de la velocidad real del loop).
#define INTERVALO_TEMP  2000

// Contador de ciclos del loop() desde la ultima interaccion activa del usuario.
// "Interaccion" = pulsacion de cualquier tecla O comando LUZ: por serial.
// Se reinicia a 0 en esos eventos y se incrementa en cada vuelta del loop.
// PROPOSITO: evitar que la actualizacion de temperatura pise el mensaje de codigo
// en la linea 1 del LCD mientras el usuario esta digitando (el codigo necesita
// toda la linea 1, que es mas ancha que el espacio T:+L:).
// Se inicializa en 9999 (> TIEMPO_MOSTRAR_INTERACCION) para que al encender
// la temperatura se muestre inmediatamente, sin esperar 12000 ciclos.
static uint16_t ultima_interaccion = 9999;

// Umbral de ciclos sin interaccion antes de que la temperatura pueda actualizar el LCD.
// Logica: si ultima_interaccion >= TIEMPO_MOSTRAR_INTERACCION → linea 1 libre → mostrar temp.
//         si ultima_interaccion <  TIEMPO_MOSTRAR_INTERACCION → usuario activo → NO pisar LCD.
// Aumentar este valor hace que el mensaje de codigo/luz permanezca mas tiempo visible
// antes de que la temperatura vuelva a aparecer en la mitad izquierda de la linea 1.
#define TIEMPO_MOSTRAR_INTERACCION  12000


// ─────────────────────────────────────────────────────────────────────────────
// a_mayusculas(s)
// Convierte la cadena 's' a mayusculas en el lugar (modifica el original).
// RAZON: strncmp() en procesar_comando_serial() es case-sensitive.
//        Sin esta conversion, "arm", "Arm" y "ARM" serian cadenas distintas.
//        Al normalizar antes de comparar, se acepta cualquier casing del usuario.
// PARAMETRO: char* s — puntero al string a convertir (debe ser mutable, no literal).
// ─────────────────────────────────────────────────────────────────────────────
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

    // Cualquier tecla pulsada cuenta como "interaccion activa".
    // Reiniciar el contador a 0 para que la temperatura NO pise la linea 1 del LCD
    // mientras el usuario esta en medio de digitar un codigo o ajustar el dimmer.
    ultima_interaccion = 0;

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

            // Mostrar el codigo enmascarado en TODA la linea 1 del LCD.
            // "Codigo: ****" ocupa 12 columnas, mas largo que el espacio de T:+L: (15 cols),
            // por eso pisa temporalmente la temperatura y la luz mientras se digita.
            lcd_goto(1, 0);               // mover cursor a linea 1, columna 0
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
            lcd_goto(0, 0);              // ir a linea 0 (exclusiva del estado de alarma)
            lcd_string("ALARMA: ARMADA  "); // 16 caracteres exactos — llena la linea completa
            usart_enviar_string("OK:ARMADA"); // confirmar por serial
        } else {
            lcd_goto(0, 0);
            lcd_string("CODIGO INCORRECTO"); // mensaje de error — 17 chars, desborda 1 (no critico en LCD)
            usart_enviar_string("ERROR:CODIGO");
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
            lcd_goto(0, 0);
            lcd_string("ALARMA: DESACTIV"); // 16 caracteres — llena la linea 0 completamente
            usart_enviar_string("OK:DESACTIVADA");
        } else {
            lcd_goto(0, 0);
            lcd_string("CODIGO INCORRECTO");
            usart_enviar_string("ERROR:CODIGO");
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
        lcd_string("L:");        // prefijo corto que cabe en el espacio disponible (7 cols)
        lcd_int(dimmer_get());   // valor actual: 0-10 (1 o 2 digitos)
        lcd_string("/10");       // denominador fijo para indicar la escala

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

        lcd_goto(1, 9);   // mitad derecha de linea 1, igual que en tecla C
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
//   "ARM"            → armar alarma de acceso
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

    // ── ARM ──────────────────────────────────────────────────────────────────
    // strncmp(comando, "ARM", 3): compara los 3 primeros caracteres del comando.
    // Si es "ARM" (o "ARM" seguido de cualquier cosa), retorna 0 → condicion verdadera.
    if (strncmp(comando, "ARM", 3) == 0) {

        alarma_armar();          // habilitar INT2/INT3 y encender LED verde (PB6)
        lcd_goto(0, 0);         // ir a la linea 0 del LCD (exclusiva de estado de alarma)
        lcd_string("ALARMA: ARMADA  "); // 16 chars exactos para llenar la linea
        usart_enviar_string("OK:ARMADA");
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
            lcd_goto(0, 0);
            lcd_string("ALARMA: DESACTIV"); // 16 chars — llena la linea 0
            usart_enviar_string("OK:DESACTIVADA");
        } else {
            // Codigo incorrecto: no se ejecuta ninguna accion sobre la alarma
            usart_enviar_string("ERROR:CODIGO");
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
        ultima_interaccion = 0;

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

        // Informar que el movimiento termino
        lcd_goto(1, 0);
        lcd_string("GARAJE ABIERTO  "); // 16 chars
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
        lcd_string("GARAJE CERRADO  "); // 16 chars
        usart_enviar_string("OK:GARAJE_CERRADO");
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
    motor_init();     // Fase 6a: configura Puerto G bits 0-3 como salidas para las bobinas
    temp_init();      // Fase 6b: configura ADC (canal 9, AVCC), pines calefactor PK3 y ventilador PK4
    dimmer_init();    // Fase 6c: configura Timer1 Fast PWM en OC1A (PB5/D11)

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
    lcd_string("T:--C   L:0/10"); // layout inicial: temperatura placeholder + luz en 0

    usart_enviar_newline(); // linea en blanco para separar el encabezado en la terminal
    // Enviar el menu de comandos disponibles a la terminal al encender
    usart_enviar_string("Comandos: ARM / DISARM:xxxx / LUZ:0-10 / GARAJE:ABRIR / GARAJE:CERRAR");
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

        // Buffer local para la linea recibida. 32 bytes = USART_RX_BUF_SIZE.
        // 'char comando[32]': array en el stack (SRAM local de esta funcion).
        char comando[32];

        // usart_leer_linea(): copia los bytes del buffer circular hasta '\n'
        // al array 'comando' y agrega '\0'. Consume los datos del buffer.
        usart_leer_linea(comando);

        // Normalizar a mayusculas para que strncmp en procesar_comando_serial
        // funcione sin importar el casing del usuario ("arm" = "ARM" = "Arm").
        a_mayusculas(comando);

        // Parsear y ejecutar el comando recibido
        procesar_comando_serial(comando);
    }

    // ── 4. TEMPERATURA Y TEMPORIZADO (Fase 6) ────────────────────────────────
    // Incrementar los dos contadores en cada vuelta del loop.
    // No se usan delays ni timers adicionales — el propio paso del tiempo
    // del loop sirve como base de tiempo imprecisa pero funcional.
    contador_temp++;       // avanzar hacia el umbral de lectura de temperatura
    ultima_interaccion++;  // avanzar hacia el umbral de "usuario inactivo"

    // Cuando el contador de temperatura alcanza INTERVALO_TEMP (2000 ciclos):
    if (contador_temp >= INTERVALO_TEMP) {

        contador_temp = 0; // reiniciar el contador para el proximo intervalo

        // CONDICION DEL LCD: solo actualizar la mitad izquierda de la linea 1
        // si el usuario lleva suficientes ciclos sin interactuar con el codigo/luz.
        // Si ultima_interaccion < TIEMPO_MOSTRAR_INTERACCION, el usuario
        // acaba de pulsar una tecla o ajustar la luz — NO pisar el mensaje en LCD.
        // IMPORTANTE: temp_controlar() (calefactor/ventilador) se ejecuta SIEMPRE
        // independientemente de esta condicion — la seguridad termica no se puede
        // pausar solo porque el usuario este digitando un codigo.
        if (ultima_interaccion >= TIEMPO_MOSTRAR_INTERACCION) {

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
}
