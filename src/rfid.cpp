#include <avr/io.h>
#include <util/delay.h>
#include <string.h>
#include "../include/rfid.h"
#include "../include/spi_master.h"
#include "../include/eeprom_mgr.h"
#include "../include/acceso.h"

// Declaraciones externas de usart (evita dependencia circular)
extern void usart_enviar_string(const char* s);
extern void usart_enviar_int(int16_t n);
extern void usart_enviar_newline();
extern void lcd_goto(uint8_t row, uint8_t col);
extern void lcd_string(const char* s);

// Llave por defecto para autenticacion MIFARE (todo 0xFF)
static const uint8_t LLAVE_MIFARE[6] = LLAVE_DEFECTO;

// Estado interno de la maquina de estados no bloqueante
#define RFID_ESTADO_OCIOSO      0
#define RFID_ESTADO_TARJETA_OK  1
#define RFID_ESTADO_PROCESANDO  2

static uint8_t  estado_rfid = RFID_ESTADO_OCIOSO;
static uint8_t  uid_actual[4];

// Operacion pendiente (enrolar/borrar/recarga)
#define RFID_OP_NINGUNA   0
#define RFID_OP_ENROLAR   1
#define RFID_OP_BORRAR    2
#define RFID_OP_RECARGAR  3

static uint8_t  op_pendiente = RFID_OP_NINGUNA;
static uint8_t  op_tipo     = 0;   // 0=adulto, 1=hijo (para ENROLAR)
static uint8_t  op_cupos    = 0;   // cupos iniciales (para ENROLAR HIJO) o cantidad a recargar

// -- Acceso a registros del RC522 por SPI ----------------------------------
static inline void rc522_escribir_reg(uint8_t reg, uint8_t valor) {
    spi_cs_bajar();
    spi_transferir(reg << 1);          // bit 0 = 0 indica escritura
    spi_transferir(valor);
    spi_cs_subir();
}

static inline uint8_t rc522_leer_reg(uint8_t reg) {
    uint8_t valor;
    spi_cs_bajar();
    spi_transferir((reg << 1) | 0x80); // bit 7 = 1 indica LECTURA (protocolo MFRC522)
    valor = spi_transferir(0x00);
    spi_cs_subir();
    return valor;
}

// -- Enviar comando al RC522 y esperar que termine -------------------------
static void rc522_enviar_cmd(uint8_t cmd) {
    // Limpiar bits de interrupcion
    rc522_escribir_reg(RC522_REG_COM_IRQ, 0x80);
    // Escribir el comando
    rc522_escribir_reg(RC522_REG_COMMAND, cmd);
    // Esperar a que el comando termine (bit IdleIRq en COMIRQ)
    if (cmd == RC522_CMD_SOFT_RESET) return;  // reset no necesita espera
    while (!(rc522_leer_reg(RC522_REG_COM_IRQ) & 0x10));
}

// -- Escribir datos en el FIFO del RC522 -----------------------------------
static void rc522_escribir_fifo(const uint8_t* datos, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        rc522_escribir_reg(RC522_REG_FIFO_DATA, datos[i]);
    }
}

// -- Leer datos del FIFO del RC522 -----------------------------------------
static void rc522_leer_fifo(uint8_t* buffer, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        buffer[i] = rc522_leer_reg(RC522_REG_FIFO_DATA);
    }
}

// -- Transceive: enviar datos y recibir respuesta --------------------------
// Retorna 1 si OK, 0 si error
static uint8_t rc522_transceive(const uint8_t* tx, uint8_t tx_len,
                                 uint8_t* rx, uint8_t* rx_len) {
    // Vaciar FIFO
    rc522_escribir_reg(RC522_REG_FIFO_LEVEL, 0x80);

    // Cargar datos a transmitir
    rc522_escribir_fifo(tx, tx_len);

    // Configurar numero de bits del ultimo byte (7 bits para REQA, 8 para el resto)
    rc522_escribir_reg(RC522_REG_BIT_FRAMING, 0x00);

    // Limpiar interrupciones y ejecutar Transceive
    rc522_escribir_reg(RC522_REG_COM_IRQ, 0x7F);
    rc522_escribir_reg(RC522_REG_COMMAND, RC522_CMD_TRANSCEIVE);

    // Activar la transmision seteando bit StartSend en BitFramingReg
    rc522_escribir_reg(RC522_REG_BIT_FRAMING, 0x80);

    // Esperar RxIRq(0x20) | IdleIRq(0x10) | TimerIRq(0x01)
    uint16_t timeout = 0;
    while (!(rc522_leer_reg(RC522_REG_COM_IRQ) & 0x31)) {
        timeout++;
        if (timeout > 3000) return 0;
    }

    // Verificar errores
    uint8_t error = rc522_leer_reg(RC522_REG_ERROR);
    if (error & 0x1B) return 0;  // buffer overflow, parity, protocol, etc.

    // Leer cuantos bytes hay en FIFO
    uint8_t nivel = rc522_leer_reg(RC522_REG_FIFO_LEVEL);

    // Leer la respuesta
    if (nivel > 0 && rx != NULL && rx_len != NULL) {
        if (nivel > *rx_len) nivel = *rx_len;
        rc522_leer_fifo(rx, nivel);
        *rx_len = nivel;
    }

    return 1;
}

// -- Transceive para REQA (formato especial con 7 bits) --------------------
static uint8_t rc522_transceive_reqa(uint8_t* respuesta) {
    // Detener cualquier comando previo
    rc522_escribir_reg(RC522_REG_COMMAND, RC522_CMD_IDLE);

    // Vaciar FIFO
    rc522_escribir_reg(RC522_REG_FIFO_LEVEL, 0x80);

    // Cargar comando REQA
    uint8_t cmd = PICC_CMD_REQA;
    rc522_escribir_fifo(&cmd, 1);

    // BitFraming: ultimo byte tiene 7 bits
    rc522_escribir_reg(RC522_REG_BIT_FRAMING, 0x07);

    // Limpiar interrupciones: Set1=0 + todos bits=1 → limpia todos los flags
    rc522_escribir_reg(RC522_REG_COM_IRQ, 0x7F);
    rc522_escribir_reg(RC522_REG_COMMAND, RC522_CMD_TRANSCEIVE);

    // Activar transmision
    rc522_escribir_reg(RC522_REG_BIT_FRAMING, 0x87);  // 0x80 (start) | 0x07 (7 bits)

    // Esperar RxIRq(0x20) | IdleIRq(0x10) | TimerIRq(0x01)
    uint16_t timeout = 0;
    while (!(rc522_leer_reg(RC522_REG_COM_IRQ) & 0x31)) {
        timeout++;
        if (timeout > 3000) break;
    }

    uint8_t irq   = rc522_leer_reg(RC522_REG_COM_IRQ);
    uint8_t nivel = rc522_leer_reg(RC522_REG_FIFO_LEVEL);

    // Detener TRANSCEIVE
    rc522_escribir_reg(RC522_REG_COMMAND, RC522_CMD_IDLE);

    // Solo hay tarjeta si RxIRq (bit 5) se activo — sin CRC check (ATQA es short frame)
    if (!(irq & 0x20)) return 0;

    if (nivel > 0 && respuesta != NULL) {
        respuesta[0] = rc522_leer_reg(RC522_REG_FIFO_DATA);
    }
    return (nivel > 0);
}

// -- Autenticar con la tarjeta para acceder a un bloque --------------------
static uint8_t rc522_autenticar(uint8_t bloque, const uint8_t* uid) {
    // Limpiar Crypto1On
    rc522_escribir_reg(RC522_REG_STATUS2, 0x00);

    // Cargar llave (6 bytes) + UID (4 bytes) en FIFO
    rc522_escribir_reg(RC522_REG_FIFO_LEVEL, 0x80);
    rc522_escribir_fifo(LLAVE_MIFARE, 6);
    rc522_escribir_fifo(uid, 4);

    // Ejecutar comando MFAuthent
    rc522_escribir_reg(RC522_REG_COM_IRQ, 0x7F);
    rc522_escribir_reg(RC522_REG_COMMAND, RC522_CMD_MFAUTHENT);
    rc522_escribir_reg(RC522_REG_BIT_FRAMING, 0x00);

    // Esperar IdleIRq(0x10) | ErrIRq(0x02) | TimerIRq(0x01)
    uint16_t timeout = 0;
    for (;;) {
        uint8_t irq = rc522_leer_reg(RC522_REG_COM_IRQ);
        if (irq & 0x13) break;
        timeout++;
        if (timeout > 3000) return 0;
    }

    // Verificar que Crypto1On este activo
    if (!(rc522_leer_reg(RC522_REG_STATUS2) & 0x08)) return 0;

    uint8_t error = rc522_leer_reg(RC522_REG_ERROR);
    if (error & 0x1B) return 0;

    return 1;
}

// ==========================================================================
// API PUBLICA
// ==========================================================================

// -- rc522_halt() -----------------------------------------------------------
// Envia HALT a la tarjeta. Despues de HALT la tarjeta no responde a REQA
// (solo a WUPA), evitando re-deteccion en el siguiente ciclo de sondeo.
static void rc522_halt() {
    // Desactivar Crypto1 si quedo activo tras autenticacion
    rc522_escribir_reg(RC522_REG_STATUS2, 0x00);

    // Calcular CRC-A de la trama [0x50, 0x00]
    rc522_escribir_reg(RC522_REG_COMMAND, RC522_CMD_IDLE);
    rc522_escribir_reg(RC522_REG_FIFO_LEVEL, 0x80);
    rc522_escribir_reg(RC522_REG_FIFO_DATA, 0x50);
    rc522_escribir_reg(RC522_REG_FIFO_DATA, 0x00);
    rc522_escribir_reg(RC522_REG_COMMAND, 0x03);       // CalcCRC
    for (uint8_t i = 0; i < 255; i++) {
        if (rc522_leer_reg(0x05) & 0x04) break;        // DivIrqReg: CRCIRq
    }
    rc522_escribir_reg(RC522_REG_COMMAND, RC522_CMD_IDLE);
    uint8_t crc_l = rc522_leer_reg(0x22);              // CRCResultRegL
    uint8_t crc_h = rc522_leer_reg(0x21);              // CRCResultRegH

    // Transmitir HALT [0x50, 0x00, CRC_L, CRC_H] — sin esperar respuesta
    rc522_escribir_reg(RC522_REG_COM_IRQ, 0x7F);
    rc522_escribir_reg(RC522_REG_FIFO_LEVEL, 0x80);
    rc522_escribir_reg(RC522_REG_FIFO_DATA, 0x50);
    rc522_escribir_reg(RC522_REG_FIFO_DATA, 0x00);
    rc522_escribir_reg(RC522_REG_FIFO_DATA, crc_l);
    rc522_escribir_reg(RC522_REG_FIFO_DATA, crc_h);
    rc522_escribir_reg(RC522_REG_COMMAND, RC522_CMD_TRANSCEIVE);
    rc522_escribir_reg(RC522_REG_BIT_FRAMING, 0x80);   // StartSend=1
    _delay_ms(2);
    rc522_escribir_reg(RC522_REG_COMMAND, RC522_CMD_IDLE);
    rc522_escribir_reg(RC522_REG_BIT_FRAMING, 0x00);
}

// -- rfid_init() -----------------------------------------------------------
void rfid_init() {
    // Soft reset — esperar hasta que PowerDown (bit 4 de CommandReg) se limpie
    rc522_escribir_reg(RC522_REG_COMMAND, RC522_CMD_SOFT_RESET);
    for (uint8_t i = 0; i < 3; i++) {
        _delay_ms(50);
        if (!(rc522_leer_reg(RC522_REG_COMMAND) & 0x10)) break;
    }

    // Configurar timer del RC522 para timeout de comunicacion
    rc522_escribir_reg(0x2A, 0x8D);  // TModeReg: TAuto=1
    rc522_escribir_reg(0x2B, 0x07);  // TPrescalerReg
    rc522_escribir_reg(0x2C, 0x00);  // TReloadRegH
    rc522_escribir_reg(0x2D, 0x3F);  // TReloadRegL (~25ms)

    // Modulacion 100% ASK — igual que la libreria MFRC522 (PCD_Init)
    // Sin esto la senal RF es debil y las tarjetas no detectan el REQA
    rc522_escribir_reg(0x15, 0x40);  // TxASKReg: Force100ASK=1

    // Preset CRC a 0x6363 segun ISO 14443-3
    rc522_escribir_reg(0x11, 0x3D);  // ModeReg

    // Encender antena (TX1 + TX2)
    uint8_t tx_ctrl = rc522_leer_reg(RC522_REG_TX_CONTROL);
    if (!(tx_ctrl & 0x03)) {
        rc522_escribir_reg(RC522_REG_TX_CONTROL, tx_ctrl | 0x03);
    }

    estado_rfid = RFID_ESTADO_OCIOSO;
}

// -- rfid_hay_tarjeta() ----------------------------------------------------
uint8_t rfid_hay_tarjeta() {
    uint8_t atqa[2];

    // Intentar REQA; si hay respuesta, hay tarjeta
    if (rc522_transceive_reqa(atqa)) {
        return 1;
    }
    return 0;
}

// -- rfid_leer_uid() -------------------------------------------------------
uint8_t rfid_leer_uid(uint8_t* uid) {
    uint8_t tx_buf[2];
    uint8_t rx_buf[5];  // 4 UID + 1 BCC
    uint8_t rx_len;

    // Anticolision cascade level 1: comando 0x93 + NVB 0x20
    tx_buf[0] = PICC_CMD_ANTICOLL_CL1;
    tx_buf[1] = 0x20;  // NVB: todos los bits validos (2 bytes enviados)

    rx_len = sizeof(rx_buf);
    if (!rc522_transceive(tx_buf, 2, rx_buf, &rx_len)) return 0;
    if (rx_len < 5) return 0;

    // Verificar BCC (XOR de los 4 bytes del UID debe ser el 5o byte)
    uint8_t bcc_calc = rx_buf[0] ^ rx_buf[1] ^ rx_buf[2] ^ rx_buf[3];
    if (bcc_calc != rx_buf[4]) return 0;

    // Copiar UID
    uid[0] = rx_buf[0];
    uid[1] = rx_buf[1];
    uid[2] = rx_buf[2];
    uid[3] = rx_buf[3];

    return 1;
}

// -- rfid_leer_contador() --------------------------------------------------
uint8_t rfid_leer_contador(uint8_t* uid, uint8_t* valor) {
    uint8_t bloque[16];

    // Autenticar con key A para el bloque del contador
    if (!rc522_autenticar(TARJETA_BLOQUE_CONTADOR, uid)) return 0;

    // Comando READ: enviar 0x30 + direccion de bloque
    uint8_t tx[2] = {PICC_CMD_READ, TARJETA_BLOQUE_CONTADOR};
    uint8_t rx_len = sizeof(bloque);

    if (!rc522_transceive(tx, 2, bloque, &rx_len)) return 0;
    if (rx_len < 1) return 0;

    // El contador esta en el primer byte del bloque
    *valor = bloque[0];
    return 1;
}

// -- rfid_escribir_contador() ----------------------------------------------
uint8_t rfid_escribir_contador(uint8_t* uid, uint8_t valor) {
    uint8_t bloque[16];

    // Autenticar
    if (!rc522_autenticar(TARJETA_BLOQUE_CONTADOR, uid)) return 0;

    // Primero leer el bloque actual para preservar los demas bytes
    uint8_t tx_read[2] = {PICC_CMD_READ, TARJETA_BLOQUE_CONTADOR};
    uint8_t rx_len = sizeof(bloque);
    if (!rc522_transceive(tx_read, 2, bloque, &rx_len)) return 0;
    if (rx_len < 16) return 0;

    // Modificar solo el primer byte (contador)
    bloque[0] = valor;
    // NOTA: los bytes 1-15 se conservan tal cual estaban

    // Comando WRITE: 0xA0 + direccion de bloque + 16 bytes de datos
    uint8_t tx_write[18];
    tx_write[0] = PICC_CMD_WRITE;
    tx_write[1] = TARJETA_BLOQUE_CONTADOR;
    for (uint8_t i = 0; i < 16; i++) {
        tx_write[2 + i] = bloque[i];
    }

    // Escribir el bloque
    if (!rc522_transceive(tx_write, 18, NULL, NULL)) return 0;

    return 1;
}

// ==========================================================================
// OPERACIONES PENDIENTES (enrolar/borrar/recarga)
// ==========================================================================

// -- rfid_entrar_modo_enrol() ---------------------------------------------
void rfid_entrar_modo_enrol(uint8_t tipo, uint8_t cupos) {
    op_pendiente = RFID_OP_ENROLAR;
    op_tipo = tipo;
    op_cupos = cupos;
}

// -- rfid_entrar_modo_borrar() --------------------------------------------
void rfid_entrar_modo_borrar() {
    op_pendiente = RFID_OP_BORRAR;
}

// -- rfid_entrar_modo_recarga() -------------------------------------------
void rfid_entrar_modo_recarga(uint8_t cantidad) {
    op_pendiente = RFID_OP_RECARGAR;
    op_cupos = cantidad;
}

// ==========================================================================
// PROCESAMIENTO PRINCIPAL
// ==========================================================================

// -- manejar_tarjeta_ok() ---------------------------------------------------
static void manejar_tarjeta_ok() {
    // Leer UID
    if (!rfid_leer_uid(uid_actual)) {
        estado_rfid = RFID_ESTADO_OCIOSO;
        return;
    }

    // ── SI HAY OPERACION PENDIENTE, procesarla y salir ──────────────────
    if (op_pendiente != RFID_OP_NINGUNA) {

        if (op_pendiente == RFID_OP_ENROLAR) {
            // Enrolar la tarjeta presentada
            uint8_t res = eeprom_guardar_uid(uid_actual, op_tipo);
            if (res != 0xFF) {
                // Guardado exitoso
                if (op_tipo == 0) {
                    usart_enviar_string("OK:ENROLADO_ADULTO");
                    usart_enviar_newline();
                    lcd_goto(1, 0); lcd_string("ENROLADO ADULTO ");
                } else {
                    usart_enviar_string("OK:ENROLADO_HIJO,");
                    usart_enviar_int(op_cupos);
                    usart_enviar_newline();
                    lcd_goto(1, 0); lcd_string("ENROLADO HIJO   ");

                    // Escribir cupos iniciales en la tarjeta
                    rfid_escribir_contador(uid_actual, op_cupos);
                }
            } else {
                usart_enviar_string("ERROR:EEPROM_LLENA_O_DUPLICADO");
                usart_enviar_newline();
            }

        } else if (op_pendiente == RFID_OP_BORRAR) {
            if (eeprom_borrar_uid(uid_actual)) {
                usart_enviar_string("OK:BORRADO");
                usart_enviar_newline();
                lcd_goto(1, 0); lcd_string("TARJETA BORRADA ");
            } else {
                usart_enviar_string("ERROR:UID_NO_EXISTE");
                usart_enviar_newline();
            }

        } else if (op_pendiente == RFID_OP_RECARGAR) {
            // Buscar UID en EEPROM
            uint8_t idx = eeprom_buscar_uid(uid_actual);
            if (idx != 0xFF) {
                uint16_t dir = EEPROM_BASE_UIDS + (idx * 5);
                uint8_t tipo = eeprom_leer(dir + 4);

                if (tipo == 1) {  // solo hijos tienen contador recargable
                    uint8_t contador = 0;
                    if (rfid_leer_contador(uid_actual, &contador)) {
                        uint8_t nuevo = contador + op_cupos;
                        if (nuevo < contador) nuevo = 255;  // saturar si desborda
                        if (rfid_escribir_contador(uid_actual, nuevo)) {
                            usart_enviar_string("OK:ACCESOS,");
                            usart_enviar_int(nuevo);
                            usart_enviar_newline();
                        } else {
                            usart_enviar_string("ERROR:ESCRIBIR_CONTADOR");
                            usart_enviar_newline();
                        }
                    } else {
                        usart_enviar_string("ERROR:LEER_CONTADOR");
                        usart_enviar_newline();
                    }
                } else {
                    usart_enviar_string("ERROR:UID_NO_ES_HIJO");
                    usart_enviar_newline();
                }
            } else {
                usart_enviar_string("ERROR:UID_NO_EXISTE");
                usart_enviar_newline();
            }
        }

        // Limpiar operacion pendiente
        op_pendiente = RFID_OP_NINGUNA;
        rc522_halt();
        estado_rfid = RFID_ESTADO_PROCESANDO;
        return;
    }

    // ── SIN OPERACION PENDIENTE: flujo normal de acceso ─────────────────
    uint8_t indice = eeprom_buscar_uid(uid_actual);

    if (indice == 0xFF) {
        acceso_denegado();
        usart_enviar_string("ACCESO:DENEGADO");
        usart_enviar_newline();
        lcd_goto(1, 0); lcd_string("ACCESO DENEGADO ");
    } else {
        uint16_t dir = EEPROM_BASE_UIDS + (indice * 5);
        uint8_t tipo = eeprom_leer(dir + 4);

        if (tipo == 0) {
            acceso_concedido_adulto();
            usart_enviar_string("ACCESO:CONCEDIDO_ADULTO");
            usart_enviar_newline();
            lcd_goto(1, 0); lcd_string("ACCESO ADULTO   ");
        } else {
            uint8_t contador = 0;
            if (rfid_leer_contador(uid_actual, &contador)) {
                if (contador > 0) {
                    contador--;
                    if (rfid_escribir_contador(uid_actual, contador)) {
                        acceso_concedido_hijo();
                        usart_enviar_string("ACCESO:CONCEDIDO_HIJO,");
                        usart_enviar_int(contador);
                        usart_enviar_newline();
                        lcd_goto(1, 0); lcd_string("ACCESO HIJO     ");
                    } else {
                        acceso_denegado();
                        usart_enviar_string("ERROR:ESCRIBIR_CONTADOR");
                        usart_enviar_newline();
                    }
                } else {
                    acceso_denegado();
                    usart_enviar_string("ACCESO:DENEGADO_SIN_CUPOS");
                    usart_enviar_newline();
                    lcd_goto(1, 0); lcd_string("SIN CUPOS       ");
                }
            } else {
                acceso_denegado();
                usart_enviar_string("ERROR:LEER_CONTADOR");
                usart_enviar_newline();
            }
        }
    }

    rc522_halt();
    estado_rfid = RFID_ESTADO_PROCESANDO;
}

// -- rfid_verificar() ------------------------------------------------------
// Maquina de estados no bloqueante llamada desde loop()
// El sondeo de tarjeta se limita a ~1 vez cada 100ms para no bloquear el loop:
// cada llamada a rfid_hay_tarjeta() tarda ~30-60ms (SPI a 1MHz + timer RC522),
// por lo que llamarla en cada vuelta del loop destruye la respuesta del teclado.
#define RFID_CICLOS_POLL    5     // sondear 1 de cada N llamadas en estado OCIOSO
#define RFID_ESPERA_CIERRE  2000  // iteraciones en PROCESANDO antes de cerrar puerta
static uint8_t  rfid_poll_cnt  = 0;
static uint16_t rfid_proc_wait = 0;

void rfid_verificar() {

    switch (estado_rfid) {

        case RFID_ESTADO_OCIOSO:
            if (++rfid_poll_cnt < RFID_CICLOS_POLL) break;
            rfid_poll_cnt = 0;
            if (rfid_hay_tarjeta()) {
                estado_rfid = RFID_ESTADO_TARJETA_OK;
            }
            break;

        case RFID_ESTADO_TARJETA_OK:
            manejar_tarjeta_ok();
            break;

        case RFID_ESTADO_PROCESANDO:
            if (++rfid_proc_wait < RFID_ESPERA_CIERRE) break;
            rfid_proc_wait = 0;
            acceso_cerrar_principal();
            lcd_goto(1, 0); lcd_string("                ");
            estado_rfid = RFID_ESTADO_OCIOSO;
            break;
    }
}
