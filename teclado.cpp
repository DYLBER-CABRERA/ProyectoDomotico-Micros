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

// Incluir el propio header — importa las definiciones de pines
// y los prototipos de las funciones públicas
#include "teclado.h"


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

// barrido anterior. 0xFF significa "ninguna tecla presionada".
// Esto es la clave de la correccion: permite distinguir entre
// "se acaba de presionar" (evento nuevo) y "sigue sostenida
// desde el barrido anterior" (NO es un evento nuevo, se ignora).
static volatile uint8_t tecla_fisica_anterior = 0xFF;

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
    _delay_us(5);

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
// LOGICA CORREGIDA: solo reporta una tecla cuando hay una transicion
// de "nada presionado" a "algo presionado". Si la misma tecla sigue
// presionada en el siguiente barrido, NO se vuelve a reportar.
ISR(TIMER2_COMPA_vect) {

    // Leer el estado fisico actual del teclado (sin logica de eventos)
    uint8_t tecla_fisica_actual = escanear_crudo();

    // Comparar contra lo que habia en el barrido ANTERIOR
    if (tecla_fisica_actual != tecla_fisica_anterior) {

        // Hubo un CAMBIO de estado fisico (alguien solto o presiono algo)

        if (tecla_fisica_actual != 0xFF) {
            // El cambio fue: de "nada" a "una tecla nueva presionada"
            // Esto SI es un evento valido de pulsacion

            // Solo reportar si el loop ya consumio la tecla anterior
            if (!hay_tecla) {
                uint8_t fila = tecla_fisica_actual / 4;
                uint8_t col  = tecla_fisica_actual % 4;

                tecla_pendiente = mapa[fila][col];
                hay_tecla       = 1;
            }
        }
        // Si tecla_fisica_actual == 0xFF, significa que SOLTARON
        // la tecla. No se reporta nada en ese caso -- solo actualiza
        // el "recuerdo" para permitir la siguiente pulsacion

        // Actualizar el recuerdo para el siguiente barrido
        tecla_fisica_anterior = tecla_fisica_actual;
    }
    // Si tecla_fisica_actual == tecla_fisica_anterior, la tecla
    // sigue sostenida desde el barrido anterior (o sigue sin
    // presionarse nada) -- en ambos casos NO se hace nada nuevo,
    // evitando exactamente el bug de las repeticiones
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
    TCCR2B = (1 << CS22) | (1 << CS20);

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
