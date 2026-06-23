#ifndef RFID_H
#define RFID_H
#include <avr/io.h>

#define UID_LEN 4

// Registros del RC522
#define RC522_REG_COMMAND       0x01
#define RC522_REG_COM_IRQ      0x04
#define RC522_REG_ERROR        0x06
#define RC522_REG_STATUS2      0x08
#define RC522_REG_FIFO_DATA    0x09
#define RC522_REG_FIFO_LEVEL   0x0A
#define RC522_REG_BIT_FRAMING  0x0D
#define RC522_REG_COLL         0x0E
#define RC522_REG_TX_CONTROL   0x14
#define RC522_REG_VERSION      0x37

// Comandos del RC522
#define RC522_CMD_IDLE         0x00
#define RC522_CMD_TRANSCEIVE   0x0C
#define RC522_CMD_MFAUTHENT    0x0E
#define RC522_CMD_SOFT_RESET   0x0F

// Comandos para la tarjeta MIFARE
#define PICC_CMD_REQA          0x26
#define PICC_CMD_ANTICOLL_CL1  0x93
#define PICC_CMD_AUTH_KEY_A    0x60
#define PICC_CMD_READ          0x30
#define PICC_CMD_WRITE         0xA0

// Parametros de la tarjeta
#define TARJETA_BLOQUE_CONTADOR  4  // bloque 4 (sector 1, bloque 0)
#define LLAVE_DEFECTO {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}

void rfid_init();
uint8_t rfid_hay_tarjeta();              // 1 si hay tarjeta cerca
uint8_t rfid_leer_uid(uint8_t* uid);     // llena uid[4], retorna 1 si OK
uint8_t rfid_leer_contador(uint8_t* uid, uint8_t* valor);
uint8_t rfid_escribir_contador(uint8_t* uid, uint8_t valor);
void rfid_verificar();                   // llamada desde loop()

// Modos de operacion pendiente (para enrolar/borrar por serial o teclado)
void rfid_entrar_modo_enrol(uint8_t tipo, uint8_t cupos);
void rfid_entrar_modo_borrar();
void rfid_entrar_modo_recarga(uint8_t cantidad);

#endif
