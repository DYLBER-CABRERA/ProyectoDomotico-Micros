#ifndef TECLADO_H
// ── #ifndef / #define / #endif ───────────────────────────────
// "Include guard": evita que este archivo se procese dos veces
// si dos módulos distintos lo incluyen. Sin esto el compilador
// vería las declaraciones duplicadas y daría error.
#define TECLADO_H

// teclado.h - Driver teclado matricial 4x4
// Sin libreria - registros directos ATmega2560
//
// PINES DEFINITIVOS (puertos independientes - sin conflicto):
//
//   FILAS    -> Puerto C bits 0-3 (entradas con pull-up)
//              PC0=pin37, PC1=pin36, PC2=pin35, PC3=pin34
//
//   COLUMNAS -> Puerto L bits 0-3 (salidas)
//              PL0=pin49, PL1=pin48, PL2=pin47, PL3=pin46
//
//   LCD      -> Puerto A completo (PA0=RS, PA2=EN, PA4-PA7=D4-D7)
//              SIN ningun pin compartido con el teclado
//
//   Fila 1 (PC0 - pin 37) → [ 1 ][ 2 ][ 3 ][ A ]
//   Fila 2 (PC1 - pin 36) → [ 4 ][ 5 ][ 6 ][ B ]
//   Fila 3 (PC2 - pin 35) → [ 7 ][ 8 ][ 9 ][ C ]
//   Fila 4 (PC3 - pin 34) → [ * ][ 0 ][ # ][ D ]
//                              ↑    ↑    ↑    ↑
//                            Col1 Col2 Col3 Col4
//                            PL0  PL1  PL2  PL3
//                           (49) (48) (47) (46)
//
// PRINCIPIO: columnas = SALIDAS, filas = ENTRADAS con pull-up
// Se activa una columna bajándola a LOW. Si hay tecla pulsada
// en esa columna, la fila cae a LOW por el contacto mecánico.
// ============================================================

// avr/io.h → define registros: DDRL, PORTL, DDRA, PORTA, PINA
// y constantes: PL0, PL1, PA0, PA1, etc.
#include <avr/io.h>

// avr/interrupt.h → necesario para que el compilador reconozca
// la macro ISR() que declara rutinas de interrupción
#include <avr/interrupt.h>

// ── Pines de COLUMNAS (Puerto L — salidas) ────────────────────
// Puerto L elegido porque no está dañado y tiene 4 bits libres
// Las columnas se activan poniéndolas en LOW durante el barrido

#define TEC_COL_DDR    DDRL    // registro dirección de datos Puerto L
#define TEC_COL_PORT   PORTL   // registro de salida Puerto L
#define TEC_COL0       PL0     // Columna 0 → pin 49 del Mega
#define TEC_COL1       PL1     // Columna 1 → pin 48 del Mega
#define TEC_COL2       PL2     // Columna 2 → pin 47 del Mega
#define TEC_COL3       PL3     // Columna 3 → pin 46 del Mega

// ── Pines de FILAS (Puerto C bits 0-3 — entradas con pull-up) ─
// Se usan los 4 bits BAJOS de Puerto C

#define TEC_FIL_DDR    DDRC    // registro dirección de datos Puerto C
#define TEC_FIL_PORT   PORTC   // registro de salida Puerto C (pull-ups)
#define TEC_FIL_PIN    PINC    // registro de LECTURA Puerto C
#define TEC_FIL_MASK   0x0F   // máscara 0000 1111 → solo bits 0-3

// ── API pública ───────────────────────────────────────────────
// Estas son las ÚNICAS funciones visibles para otros módulos.
// La lógica interna (barrido, debounce, ISR) queda en el .cpp

// Inicializa puertos, pull-ups y Timer2 para barrido automático
void teclado_init();

// Retorna el carácter de la tecla presionada ('0'-'9','A'-'D',
// '*','#') o '\0' si no hay tecla nueva disponible.
// Consume la tecla: la segunda llamada retorna '\0' hasta que
// se presione una nueva.
char teclado_leer();

// Retorna 1 si hay una tecla disponible, 0 si no.
// Útil para revisar sin consumir la tecla.
uint8_t teclado_hay();

// Cierra el include guard — todo lo declarado entre #ifndef y
// aquí solo se procesa una vez por unidad de compilación
#endif
