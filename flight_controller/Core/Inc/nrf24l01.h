#ifndef NRF24L01_H
#define NRF24L01_H

#include "main.h"
#include <stdint.h>

/* =========================================================
 *  COMMANDS
 * ========================================================= */
#define NRF_CMD_R_REGISTER 0x00
#define NRF_CMD_W_REGISTER 0x20
#define NRF_CMD_W_TX_PAYLOAD 0xA0 /* FIX: was 0b10100000, hex is clearer */
#define NRF_CMD_R_RX_PAYLOAD 0x61 /* FIX: was 0b01100001 */
#define NRF_CMD_FLUSH_TX 0xE1     /* FIX: was 0b11100001 */
#define NRF_CMD_FLUSH_RX 0xE2     /* FIX: was 0b11100010 */
#define NRF_CMD_NOP 0xFF          /* ADD: useful for reading STATUS with no side effects */

/* =========================================================
 *  REGISTERS
 * ========================================================= */
#define NRF_REG_CONFIG 0x00
#define NRF_REG_EN_AA 0x01     /* ADD: auto-acknowledgment register */
#define NRF_REG_EN_RXADDR 0x02 /* ADD: enable RX pipe addresses */
#define NRF_REG_SETUP_AW 0x03
#define NRF_REG_RF_CH 0x05
#define NRF_REG_RF_SETUP 0x06
#define NRF_REG_STATUS 0x07
#define NRF_REG_RX_ADDR_P0 0x0A
#define NRF_REG_TX_ADDR 0x10
#define NRF_REG_RX_PW_P0 0x11
#define NRF_REG_FIFO_STATUS 0x17 /* ADD: useful for debugging TX/RX FIFO state */

/* =========================================================
 *  CONFIG REGISTER BIT MASKS (for readability)
 * ========================================================= */
#define NRF_CONFIG_PRIM_RX (1 << 0) /* 1 = PRX (receiver), 0 = PTX (transmitter) */
#define NRF_CONFIG_PWR_UP (1 << 1)  /* 1 = Power up */
#define NRF_CONFIG_EN_CRC (1 << 3)  /* 1 = Enable CRC */
#define NRF_CONFIG_CRCO (1 << 2)    /* 1 = 2-byte CRC, 0 = 1-byte CRC */

/* =========================================================
 *  STATUS REGISTER BIT MASKS
 * ========================================================= */
#define NRF_STATUS_RX_DR (1 << 6)  /* Data ready in RX FIFO */
#define NRF_STATUS_TX_DS (1 << 5)  /* Data sent TX FIFO */
#define NRF_STATUS_MAX_RT (1 << 4) /* Max retransmit reached */

/* =========================================================
 *  PAYLOAD SIZE - single source of truth
 *  Keep this in sync with sizeof(RC_Packet_t) = 16
 * ========================================================= */
#define NRF_PAYLOAD_SIZE 16

/* =========================================================
 *  RC PACKET STRUCT
 *  __attribute__((packed)) is critical: prevents padding
 *  so raw SPI byte buffer maps directly to struct fields.
 *  Total: 1+1+1+1+1+1+1+1+2+1+5 = 16 bytes
 * ========================================================= */
typedef struct __attribute__((packed)) {
        uint8_t joy1_x;
        uint8_t joy1_y;
        uint8_t joy2_x;
        uint8_t joy2_y;
        uint8_t sw1;
        uint8_t sw2;
        uint8_t sw3;
        uint8_t sw4;
        uint16_t packet_id; /* 2 bytes - ensure endian match with Arduino */
        uint8_t failsafe_flag;
        uint8_t reserved[5];
} RC_Packet_t;

/* =========================================================
 *  SHARED VARIABLES (defined in nrf24l01.c)
 * ========================================================= */
extern volatile RC_Packet_t current_rc_data;
extern volatile uint32_t last_packet_time;

/* =========================================================
 *  FUNCTION PROTOTYPES
 * ========================================================= */
void NRF24_Init_Reciever(void);
void NRF24_Init_Transmitter(void);
void NRF24_EXTI_Setup(void);

uint8_t NRF24_ReadReg(uint8_t reg);
void NRF24_WriteReg(uint8_t reg, uint8_t data);
void NRF24_WriteAddr(uint8_t reg, uint8_t* addr);
void NRF24_ReadPayload(uint8_t* data);
void NRF24_Send(uint8_t* data);

/* ADD: extracted RX handler - called from BOTH ISR and polling fallback */
void NRF24_HandleRX(void);

void Debug_Print_RC(void);
void EXTI0_IRQHandler(void);

#endif /* NRF24L01_H */
