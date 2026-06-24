// ============================================================
// teclado.cpp — Driver teclado matricial 4x4
// ATmega2560 — sin librerías — registros directos
// ============================================================

// avr/io.h → acceso a registros DDRx, PORTx, PINx del micro
#include <avr/io.h>

// avr/interrupt.h → permite escribir ISR(vector) en este .cpp
// Cada .cpp es una unidad de compilación independiente — si no
// se incluye aquí, el compilador no reconoce la macro ISR()
#include <avr/interrupt.h>

// util/delay.h → permite usar _delay_ms() y _delay_us()
// Necesario para el debounce de 20ms y el settle de 5us
#include <util/delay.h>

// Incluir el propio header � importa las definiciones de pines
// y los prototipos de las funciones p�blicas
#include "../include/teclado.h"


// ============================================================
// MAPA DE TECLAS
// Arreglo bidimensional constante: mapa[fila][columna]
// const → el compilador lo pone en Flash, no en SRAM
// Coincide exactamente con la distribución física del teclado
// ============================================================
static const char mapa[4][4] = {
    {'1', '2', '3', 'A'},  // Fila 0: PA0 activa (LOW)
    {'4', '5', '6', 'B'},  // Fila 1: PA1 activa (LOW)
    {'7', '8', '9', 'C'},  // Fila 2: PA2 activa (LOW)
    {'*', '0', '#', 'D'}   // Fila 3: PA3 activa (LOW)
};


// ============================================================
// VARIABLES INTERNAS (privadas de este módulo)
// volatile → obliga al compilador a leer el valor real desde
// memoria en cada acceso, sin cachear en un registro.
// Obligatorio cuando una variable es modificada en una ISR y
// leída en el loop principal — sin volatile el compilador
// puede "optimizar" y nunca ver el cambio de la ISR.
// static → hace las variables visibles SOLO dentro de este
// archivo .cpp (privadas del módulo)
// ============================================================

// Último carácter detectado por el barrido — lo escribe la ISR
// y lo lee el loop con teclado_leer()
static volatile char    tecla_pendiente = '\0'; //Caracter detectado por ISR

// Bandera: 1 = hay tecla esperando ser procesada, 0 = sin tecla
// La ISR la pone en 1; teclado_leer() la pone en 0
static volatile uint8_t hay_tecla       = 0;// 1=tecla lista, 0=libre

// -- Estado del DEBOUNCE POR INTEGRACION (ver ISR mas abajo) -------------
// Numero de lecturas IGUALES consecutivas (cada ISR ~10ms) necesarias para
// aceptar un estado como real. 2 muestras = ~20ms: suficiente para filtrar
// el rebote de un teclado fisico sin que se sienta lento. Subelo a 3 si tu
// teclado rebota mucho; bajalo a 1 para maxima velocidad (menos robusto).
#define TEC_MUESTRAS_ESTABLE  2

static volatile uint8_t lectura_previa  = 0xFF; // ultima lectura cruda vista
static volatile uint8_t cuenta_estable  = 0;    // lecturas iguales seguidas
static volatile uint8_t tecla_reportada = 0xFF; // ultimo estado ESTABLE aceptado

// -- RELOJ DEL SISTEMA (base de tiempo REAL) ----------------------------
// La ISR del Timer2 late cada ~10ms sin importar lo rapido/lento que vaya
// el loop. Aqui llevamos los milisegundos para que el horno y la temperatura
// midan TIEMPO REAL (antes contaban "vueltas del loop", que con el RFID se
// volvieron lentas y variables). Lo leen otros modulos con millis_sistema().
static volatile uint32_t ms_sistema = 0;

// ============================================================
// escanear_columna(col)
// FUNCIÓN PRIVADA — no declarada en el .h
// static → invisible fuera de este .cpp
//
// Activa una sola columna (LOW) y lee si alguna fila respondió
// Parámetro: col = 0,1,2,3 (índice de columna)
// Retorna:   índice de fila presionada (0-3), o 0xFF si ninguna
// ============================================================
static uint8_t escanear_columna(uint8_t col) {

    // Activar solo la columna 'col' poniéndola en LOW
    // Las otras 3 columnas quedan en HIGH
    //
    // (1 << col)  → bit en 1 en la posición de la columna
    // ~(1 << col) → todos los bits en 1 EXCEPTO esa columna
    // & 0x0F      → asegura que solo afecta bits 0-3 de PORTL
    //               (no tocamos bits 4-7 que podrían estar en uso)
    //
    // Ejemplo col=1: ~(1<<1) = ~0b00000010 = 0b11111101
    //                & 0x0F = 0b00001101 → PL0=1, PL1=0, PL2=1, PL3=1
    TEC_COL_PORT = (~(1 << col)) & 0x0F;

    // Esperar 5µs para que el pin se estabilice eléctricamente
    // antes de leer — evita leer durante la transición del pin
    // En Proteus 5us bastaban (lineas ideales). En hardware real, con los
    // pull-up INTERNOS (~20-50k) y la capacitancia del cableado, la fila tarda
    // mas en estabilizarse: 20us da margen y sigue siendo despreciable (4
    // columnas = 80us por ISR). Aun mejor: pull-ups EXTERNOS de 1k-10k en las filas.
    _delay_us(20);

    // Leer el estado actual de las 4 filas
    // PINA → registro de lectura del Puerto A (estado real de pines)
    // & TEC_FIL_MASK (0x0F) → conservar solo bits 0-3 (las filas)
    //ignorar bits 4-7 (datos del LCD)
    //ignirar PC4-PC7 que podria tener otro estado
    uint8_t filas = TEC_FIL_PIN & TEC_FIL_MASK;

    // Con pull-up activo, los pines están en HIGH (1) por defecto
    // Cuando se presiona una tecla, el pin cae a LOW (0)
    // ~filas → invierte la lógica: el bit que era 0 (presionado)
    //          se convierte en 1 (más fácil de detectar)
    // & TEC_FIL_MASK → limpiar bits superiores del complemento
    uint8_t presionadas = (~filas) & TEC_FIL_MASK;

    // Si no hay ningún bit en 1, ninguna fila está activa
    if (presionadas == 0) return 0xFF;

    // Recorrer las 4 filas buscando cuál está activa
    for (uint8_t f = 0; f < 4; f++) {
        // (1 << f) → máscara con solo el bit de la fila f
        // Si ese bit está en 1 en 'presionadas' → esa fila tiene tecla
        if (presionadas & (1 << f)) {
            return f;  // retornar índice de fila (0-3)
        }
    }

    // No debería llegar aquí, pero por seguridad
    return 0xFF;
}


// ============================================================
// escanear_teclado()
// FUNCIÓN PRIVADA
// Barre las 4 columnas y retorna la tecla detectada con debounce
// Debounce: espera 20ms y confirma que la misma tecla sigue
//           presionada — filtra rebotes mecánicos del contacto
// Retorna carácter del mapa o '\0' si no hay tecla
// ============================================================
static char escanear_teclado() {

    // Recorrer cada columna (0 a 3)
    for (uint8_t col = 0; col < 4; col++) {

        // Primer escaneo: ¿hay algo en esta columna?
        uint8_t fila = escanear_columna(col);

        if (fila != 0xFF) {
            // Posible tecla detectada — aplicar debounce

            // Esperar 20ms: tiempo estándar para que el rebote
            // mecánico del contacto se estabilice
            _delay_ms(20);

            // Segundo escaneo: confirmar que sigue la misma tecla
            uint8_t fila2 = escanear_columna(col);

            if (fila2 == fila) {
                // Confirmado — la misma fila sigue activa en la
                // misma columna después de 20ms → es una pulsación real

                // Restaurar todas las columnas a HIGH (estado reposo)
                // 0x0F = 0000 1111 → PL0-PL3 en HIGH
                TEC_COL_PORT = 0x0F;

                // Retornar el carácter del mapa en esa posición
                // mapa[fila][col] → el carácter que corresponde
                return mapa[fila][col];
            }
        }
    }

    // Ninguna columna detectó tecla válida
    // Restaurar estado reposo antes de retornar
    TEC_COL_PORT = 0x0F;
    return '\0';
}

// -- escanear_teclado_crudo() --------------------------------------------
// NUEVO NOMBRE para la version que SOLO detecta que hay presionado
// AHORA MISMO, sin logica de "evento nuevo". Recorre las 4 columnas
// y retorna un codigo unico (fila*4+col) para cada tecla, o 0xFF
// si ninguna esta presionada. NO hace debounce de tiempo aqui --
// el debounce real ahora es "debe soltarse antes de repetir".
static uint8_t escanear_crudo() {

    for (uint8_t col = 0; col < 4; col++) {
        uint8_t fila = escanear_columna(col);
        if (fila != 0xFF) {
            // Codigo unico que identifica la posicion exacta
            // fila*4+col da un numero de 0 a 15, distinto para
            // cada una de las 16 teclas del teclado
            return (fila * 4) + col;
        }
    }
    return 0xFF; // ninguna tecla presionada en este instante
}

// ============================================================
// ISR(TIMER2_COMPA_vect)
// Rutina de interrupción del Timer2 — se ejecuta cada ~10ms
//OCR2A.
// El hardware detiene el programa principal, ejecuta esta funcion,
// y luego lo retoma exactamente donde estaba.
//
// REGLA DEL PROYECTO: la ISR debe ser MÍNIMA
// Solo realiza el barrido y escribe la bandera
// NO llama a lcd_string(), NO llama a usart_enviar()
// Todo el trabajo pesado ocurre en el loop principal
// ============================================================

// -- ISR(TIMER2_COMPA_vect) ----------------------------------------------
// DEBOUNCE POR INTEGRACION (robusto en hardware real):
// El teclado fisico REBOTA: al presionar/soltar, el contacto abre y cierra
// varias veces durante unos milisegundos. En Proteus eso no pasa (contacto
// ideal), por eso antes iba rapido en simulacion pero en fisico se "trababa"
// o perdia pulsaciones. Aqui NO se actua ante la lectura cruda: se exige que
// la misma lectura se repita TEC_MUESTRAS_ESTABLE veces seguidas (cada ISR =
// ~10ms) antes de aceptarla como estado real. Asi el rebote se filtra y cada
// pulsacion se registra UNA sola vez, de forma fiable y rapida (~20ms).
ISR(TIMER2_COMPA_vect) {

    // Reloj del sistema: esta ISR late cada ~10 ms (base de tiempo real).
    ms_sistema += 10;

    // Lectura cruda del estado fisico AHORA (codigo 0-15, o 0xFF si nada)
    uint8_t actual = escanear_crudo();

    // -- Integracion: contar cuantas lecturas IGUALES seguidas llevamos --
    if (actual == lectura_previa) {
        if (cuenta_estable < 255) cuenta_estable++;
    } else {
        lectura_previa = actual;   // cambio de lectura: reiniciar el conteo
        cuenta_estable = 0;
    }

    // Actuar SOLO cuando la lectura lleva EXACTAMENTE el umbral de muestras
    // estables. Usar '==' (no '>=') hace que esto se ejecute una unica vez por
    // estado estable, aunque la tecla se siga sosteniendo muchos ciclos mas
    // (asi nunca se repite la tecla sola).
    if (cuenta_estable == TEC_MUESTRAS_ESTABLE) {

        // 'actual' ya esta libre de rebotes: es el estado REAL del teclado.
        if (actual != tecla_reportada) {
            tecla_reportada = actual;   // recordar el estado estable aceptado

            // Paso de "nada" (o de otra tecla ya soltada) a una tecla nueva:
            if (actual != 0xFF) {
                // Registrar la pulsacion (si el loop ya consumio la anterior)
                if (!hay_tecla) {
                    uint8_t fila = actual / 4;
                    uint8_t col  = actual % 4;
                    tecla_pendiente = mapa[fila][col];
                    hay_tecla       = 1;
                }
            }
            // Si actual == 0xFF, la tecla se solto de forma estable: no se
            // registra nada, solo queda listo para aceptar la proxima.
        }
    }
}

// ============================================================
// teclado_init()
// Configura puertos y Timer2
// Debe llamarse UNA vez en setup(), antes de sei()
// ============================================================
void teclado_init() {

    // ── 1. COLUMNAS → Puerto L bits 0-3 como SALIDAS ─────────

    // DDRL |= 0x0F
    // 0x0F = 0000 1111 en binario
    // |=  → OR: pone bits 0-3 en 1 (salida) sin tocar bits 4-7
    // Nunca usar = 0x0F directo porque sobreescribiría bits 4-7
    DDRL |= 0x0F;

    // Estado inicial: todas las columnas en HIGH (reposo)
    // PORTL |= 0x0F → pone PL0-PL3 en 1 (HIGH)
    // Con columnas en HIGH, ninguna fila puede caer a LOW
    // aunque haya una tecla presionada → el teclado "no detecta"
    // hasta que el barrido baja una columna a LOW activamente
    PORTL |= 0x0F;

    // ── 2. FILAS → Puerto C bits 0-3 como ENTRADAS con pull-up

    // DDRC &= 0xF0 → PC0-PC3 como entradas, PC4-PC7 no se modifican
    DDRC &= 0xF0;

    // PORTC |= 0x0F → activa pull-up en PC0-PC3
    // Con pull-up: pin sin tecla = HIGH (1)
    //              pin con tecla = LOW  (0) por el contacto a GND
    PORTC |= 0x0F;

    // ── 3. TIMER2 en modo CTC → interrupción cada ~10ms ──────
    // CTC = Clear Timer on Compare Match
    // El contador sube de 0 hasta OCR2A, dispara la ISR y
    // se reinicia a 0 automáticamente — ciclo perfecto

    // TCCR2A: registro de control A del Timer2
    // WGM21 = 1 → modo CTC (Clear Timer on Compare match)
    // Los demás bits en 0 → sin salida de onda en pines
    TCCR2A = (1 << WGM21);

    // TCCR2B: registro de control B del Timer2
    // CS22=1, CS21=0, CS20=1 → prescaler 1024
    // El reloj del timer = F_CPU / 1024 = 16MHz / 1024 = 15625 Hz
    // OJO: en el Timer2, 111 = /1024 (en Timer0/1 seria distinto). Antes habia
    // 101, que en Timer2 es /128 -> la ISR latia ~8x mas rapido de lo previsto,
    // y por eso el reloj de ms (y el horno) contaban acelerados. Corregido a /1024.
    TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20);

    // OCR2A: valor de comparación
    // Para interrupción cada 10ms con prescaler 1024:
    // OCR2A = (F_CPU / prescaler / frecuencia_deseada) - 1
    // OCR2A = (16.000.000 / 1024 / 100) - 1 = 156 - 1 = 155
    // Con OCR2A = 155 el timer cuenta 0,1,2...155 y dispara
    // la ISR → 15625 ticks / 156 = ~100 veces por segundo = 10ms
    OCR2A = 155;

    // TIMSK2: registro de máscara de interrupciones del Timer2
    // OCIE2A = 1 → habilitar interrupción por comparación con A
    // Sin esto la ISR nunca se ejecuta aunque el timer cuente
    TIMSK2 = (1 << OCIE2A);

    // Limpiar estado inicial de las variables
    tecla_pendiente = '\0';  // sin tecla pendiente
    hay_tecla       = 0;     // bandera apagada
}


// ============================================================
// teclado_leer()
// Retorna la tecla disponible y CONSUME la bandera
// Segunda llamada seguida retorna '\0' hasta nueva pulsación
// ============================================================
char teclado_leer() {

    // Verificar si la ISR depositó una tecla
    if (hay_tecla) {

        // Copiar el carácter a variable local
        char c = tecla_pendiente;

        // Limpiar para que la ISR pueda detectar la próxima
        tecla_pendiente = '\0';
        hay_tecla       = 0;    // consumir la bandera

        return c;               // retornar el carácter
    }

    return '\0';  // no había tecla disponible
}


// ============================================================
// teclado_hay()
// Consulta sin consumir — útil si el loop quiere saber si
// hay tecla antes de decidir si llamar a teclado_leer()
// ============================================================
uint8_t teclado_hay() {
    // Retorna directamente el valor de la bandera
    // 0 = no hay tecla, 1 = hay tecla esperando
    return hay_tecla;
}


// ============================================================
// millis_sistema()
// Tiempo del sistema en milisegundos (base: ISR del Timer2, ~10ms).
// Sirve para medir tiempo REAL (horno, temperatura) sin depender de la
// velocidad del loop. Lectura atomica de los 4 bytes (se modifica en ISR).
// ============================================================
uint32_t millis_sistema() {
    uint32_t v;
    uint8_t  s = SREG;   // guardar estado (bit I de interrupciones)
    cli();               // leer los 4 bytes sin que la ISR los cambie a media lectura
    v = ms_sistema;
    SREG = s;            // restaurar interrupciones como estaban
    return v;
}

