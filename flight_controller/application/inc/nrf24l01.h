#ifndef NRF24L01_H
#define NRF24L01_H

#include "main.h"

/* =========================================================
 *  COMMANDS
 * ========================================================= */
#define NRF_CMD_R_REGISTER 0x00
#define NRF_CMD_W_REGISTER 0x20
#define NRF_CMD_W_TX_PAYLOAD 0xA0
#define NRF_CMD_R_RX_PAYLOAD 0x61
#define NRF_CMD_FLUSH_TX 0xE1
#define NRF_CMD_FLUSH_RX 0xE2
#define NRF_CMD_NOP 0xFF

/* =========================================================
 *  REGISTERS
 * ========================================================= */
#define NRF_REG_CONFIG 0x00
#define NRF_REG_EN_AA 0x01
#define NRF_REG_EN_RXADDR 0x02
#define NRF_REG_SETUP_AW 0x03
#define NRF_REG_SETUP_RETR 0x04
#define NRF_REG_RF_CH 0x05
#define NRF_REG_RF_SETUP 0x06
#define NRF_REG_STATUS 0x07
#define NRF_REG_OBSERVE_TX 0x08
#define NRF_REG_RPD 0x09
#define NRF_REG_RX_ADDR_P0 0x0A
#define NRF_REG_RX_ADDR_P1 0x0B
#define NRF_REG_RX_ADDR_P2 0x0C
#define NRF_REG_RX_ADDR_P3 0x0D
#define NRF_REG_RX_ADDR_P4 0x0E
#define NRF_REG_RX_ADDR_P5 0x0F
#define NRF_REG_TX_ADDR 0x10
#define NRF_REG_RX_PW_P0 0x11
#define NRF_REG_FIFO_STATUS 0x17
#define NRF_REG_DYNPD 0x1C
#define NRF_REG_FEATURE 0x1D
#define NRF_REG_TX_PW 0x22

/* =========================================================
 *  CONFIG REGISTER BIT MASKS
 * ========================================================= */
#define NRF_CONFIG_PRIM_RX (1 << 0)
#define NRF_CONFIG_PWR_UP (1 << 1)
#define NRF_CONFIG_CRCO (1 << 2)
#define NRF_CONFIG_EN_CRC (1 << 3)

/* =========================================================
 *  STATUS REGISTER BIT MASKS
 * ========================================================= */
#define NRF_STATUS_RX_DR (1 << 6)
#define NRF_STATUS_TX_DS (1 << 5)
#define NRF_STATUS_MAX_RT (1 << 4)

/* =========================================================
 *  FIFO STATUS BITS
 * ========================================================= */
#define NRF_FIFO_RX_EMPTY (1 << 0)
#define NRF_FIFO_RX_FULL (1 << 1)
#define NRF_FIFO_TX_EMPTY (1 << 4)
#define NRF_FIFO_TX_FULL (1 << 5)
#define NRF_FIFO_TX_REUSE (1 << 6)

/* =========================================================
 *  PAYLOAD SIZE
 * ========================================================= */
#define NRF_PAYLOAD_SIZE 16

/* =========================================================
 *  RC PACKET STRUCT [16 Bytes]
 * ========================================================= */
typedef struct __attribute__((packed)) {
        uint16_t joy1_x;
        uint16_t joy1_y;
        uint16_t joy2_x;
        uint16_t joy2_y;
        uint8_t fly_mode;
        uint8_t altitudeHold_flag;
        uint8_t failsafe_flag;
        uint32_t packet_id;
        uint8_t reserved;
} RC_Packet_t;

/* =========================================================
 *  RC ACK PACKET STRUCT [6 Bytes]
 * ========================================================= */
typedef struct __attribute__((packed)) {
        uint16_t battery;
        uint32_t packet_id;
        uint8_t sig_strength;
        uint8_t flags;
} ACK_Packet_t;

/* =========================================================
 *  SHARED VARIABLES
 * ========================================================= */
extern volatile RC_Packet_t rc_packet;
extern volatile ACK_Packet_t ack_packet;

extern volatile uint32_t last_packet_time;
extern volatile uint8_t isDataReady;

extern volatile uint32_t data[3];

/* =========================================================
 *  FUNCTION PROTOTYPES
 * ========================================================= */
void SPI1_Init(void);
void init_dwt(void);
void delay_us(uint32_t us);

void NRF24_Init_Receiver(void); /* FIXED: was "Reciever" */

uint8_t NRF24_ReadReg(uint8_t reg);
void NRF24_WriteReg(uint8_t reg, uint8_t data);
void NRF24_WriteAddr(uint8_t reg, uint8_t* addr);
void NRF24_ReadPayload(uint8_t* data);
void NRF24_Send(uint8_t* data);
uint8_t NRF24_IsHardwareAlive(void);
void NRF24_SendACK_Payload(void);
void NRF24_SendInitialACK_Payload(void);

uint8_t SPI1_Transfer(uint8_t tx_byte);

/* Task-safe handlers */
void NRF24_Task(void);
void NRF24_ProcessRX(void);

#define CSN_LOW() (GPIOB->BSRR = GPIO_BSRR_BR2)
#define CSN_HIGH() (GPIOB->BSRR = GPIO_BSRR_BS2)
#define CE_LOW() (GPIOB->BSRR = GPIO_BSRR_BR1)
#define CE_HIGH() (GPIOB->BSRR = GPIO_BSRR_BS1)

/* =========================================================
 *  DEBUGGING
 * ========================================================= */
#endif /* NRF24L01_H */
