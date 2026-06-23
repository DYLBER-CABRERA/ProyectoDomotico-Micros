#ifndef DIMMER_H
#define DIMMER_H

// dimmer.h - Control de iluminacion dimerizada via PWM
// Sin librerias - registros directos ATmega2560
//
// AJUSTE: el usuario confirmo que el cambio de brillo visible
// en la simulacion de Proteus solo se nota entre el 0% y el 10%
// de duty cycle -- por encima de eso, el LED se ve igual de
// brillante sin importar el valor real. Se limita el rango
// util a 0-10 (en vez de 0-100) para que cada unidad represente
// un cambio visible real.
//
// CONEXION FISICA:
//   LED dimerizado -> OC1A -> PB5 (pin 11)
//
// CONTROL: nivel 0-10 ajustable por comando serial (LUZ:0-10)
// o por teclado (tecla C sube 1, tecla D baja 1)

#include <avr/io.h>

#define DIMMER_DDR    DDRB
#define DIMMER_PIN    PB5    // OC1A, pin 11

// Rango util del dimmer (escala de usuario)
#define DIMMER_NIVEL_MAX  10

// -- CURVA DE BRILLO LOGARITMICA/PERCEPTUAL -----------------------------
// El ojo percibe el brillo de forma logaritmica (ley de Weber-Fechner): un
// mapeo LINEAL (OCR1A = nivel*k) se ve casi todo el cambio al inicio y nada
// al final. Para que cada nivel represente un paso de brillo PARECIDO al
// anterior, el duty cycle debe crecer de forma exponencial (curva gamma).
// Se usa una tabla de 11 valores (niveles 0-10 -> OCR1A 0-255) en dimmer.cpp.
// Cubre todo el rango: util tanto en Proteus (pasos finos abajo) como en el
// entrenador fisico (llega al 100% de brillo).

// -- API publica --------------------------------------------------------

void dimmer_init();

// Ajusta el nivel de brillo. nivel: 0-10 (antes era 0-100)
// 0 = apagado, 10 = brillo maximo visible en la simulacion
void dimmer_set(uint8_t nivel);

// Retorna el nivel actual configurado (0-10)
uint8_t dimmer_get();

#endif
