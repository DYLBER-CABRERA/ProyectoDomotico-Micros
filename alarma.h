#ifndef ALARMA_H
#define ALARMA_H

// alarma.h - Maquina de estados de alarmas (acceso + incendio)
// Sin librerias - registros directos ATmega2560
//
// DOS SUBSISTEMAS INDEPENDIENTES:
//   1. Alarma de ACCESO: se ARMA/DESARMA con codigo del teclado
//      Sensores: DIP switch SW1 (puerta) y SW2 (ventana/garaje)
//   2. Alarma de INCENDIO: SIEMPRE activa, prioridad maxima
//      Sensores: DIP switch SW3 y SW4 (zonas de humo)
//      No requiere codigo para activarse, solo para silenciar
//
// CONEXION FISICA (DIPSW_7, pines 1-4 al lado B con R10K a GND):
//   SW1 -> PD2 (pin 19) INT2 -> ACCESO  (puerta principal)
//   SW2 -> PD3 (pin 18) INT3 -> ACCESO  (ventana/garaje)
//   SW3 -> PE4 (pin 2)  INT4 -> INCENDIO (zona 1)
//   SW4 -> PE5 (pin 3)  INT5 -> INCENDIO (zona 2)
//
//   LED rojo (disparada)  -> PB7 (pin 13) + R220 a GND
//   LED verde (armada)    -> PB6 (pin 12) + R220 a GND

#include <avr/io.h>

// -- Estados de la alarma de ACCESO ------------------------------------
#define ALARMA_DESACTIVADA 0
#define ALARMA_ARMADA       1
#define ALARMA_DISPARADA    2

// -- Tipo de evento que disparo la alarma ------------------------------
#define ALARMA_TIPO_NINGUNO   0
#define ALARMA_TIPO_INCENDIO  1
#define ALARMA_TIPO_ACCESO    2
#define ALARMA_TIPO_INTRUSO   3   // codigo equivocado repetidas veces

// -- Anti-intrusos: intentos de codigo fallidos antes de disparar ------
// Si alguien teclea mal el codigo (al armar o desarmar) este numero de
// veces seguidas, se interpreta como intento de intrusion y se dispara
// la alarma. Un codigo correcto reinicia el contador.
#define ALARMA_MAX_INTENTOS   3

// -- Pines de los LEDs indicadores (Puerto B) --------------------------
#define LED_DISPARADA   PB7   // rojo, pin 13
#define LED_ARMADA       PB6   // verde, pin 12

// -- Codigo de armado/desarmado (4 digitos, ajustable) -----------------
#define ALARMA_CODIGO_LEN  4

// -- API publica --------------------------------------------------------

// Configura pines de LEDs, interrupciones EICRA/EIMSK e inicializa
// el estado de la alarma. Llamar en setup() ANTES de sei().
void alarma_init();

// Arma la alarma de ACCESO (solo afecta SW1/SW2 - INT2/INT3)
// El incendio (INT4/INT5) ya esta siempre vigilando desde alarma_init()
void alarma_armar();

// Desarma la alarma de ACCESO y silencia cualquier disparo activo
// (tanto de acceso como de incendio)
void alarma_desarmar();

// Debe llamarse en cada vuelta del loop() principal.
// Revisa las banderas que las ISR dejaron y actualiza LEDs/estado.
// Retorna 1 si hubo un cambio de estado que vale la pena reportar
// por USART/LCD en esta vuelta del loop, 0 si no hay nada nuevo.
uint8_t alarma_actualizar();

// Retorna el estado actual: ALARMA_DESACTIVADA/ARMADA/DISPARADA
uint8_t alarma_estado();

// Retorna que tipo de evento causo el disparo actual
// (ALARMA_TIPO_INCENDIO o ALARMA_TIPO_ACCESO), o NINGUNO si no hay disparo
uint8_t alarma_tipo_disparo();

// Verifica si el codigo ingresado coincide con el codigo de armado.
// Retorna 1 si es correcto, 0 si no.
// (la comparacion del codigo en si se hace aqui para mantener
//  la logica de seguridad encapsulada en este modulo)
uint8_t alarma_verificar_codigo(const char* codigo_ingresado);

// Registra un intento de codigo FALLIDO. Incrementa el contador interno;
// cuando alcanza ALARMA_MAX_INTENTOS dispara la alarma de intruso (LED rojo
// encendido, estado DISPARADA, tipo ALARMA_TIPO_INTRUSO) y reinicia el
// contador. Retorna 1 si este fallo provoco el disparo, 0 si aun no.
uint8_t alarma_registrar_fallo();

// Reinicia el contador de intentos fallidos. Llamar cuando el codigo es
// correcto (armado/desarmado exitoso) para empezar de cero.
void alarma_reset_intentos();

// Retorna cuantos intentos quedan antes de disparar la alarma de intruso.
uint8_t alarma_intentos_restantes();

#endif
