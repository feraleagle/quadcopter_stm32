#include "nrf24l01.h"
#include "stm32f4xx_hal_uart.h"
#include <stdint.h>

/* =========================================================
 *  SHARED VARIABLE DEFINITIONS
 * ========================================================= */
volatile RC_Packet_t current_rc_data;
volatile ACK_Payload_t current_ack_payload;
volatile uint32_t last_packet_time = 0;

/* FIX nRF24 sends/receives LSB first over the air.
 * The address bytes are sent in array order [0] first.
 * Both STM32 and Arduino must use the SAME byte order here.
 * Your Arduino: const byte address[5] = {0xE7,0xE7,0xFF,0x11,0xE2}
 * This matches - no change needed. */
uint8_t DroneAddr[] = {0xE7, 0xE7, 0xFF, 0x11, 0xE2};

/* =========================================================
 *  HARDWARE MACROS
 *  Using BSRR for atomic bit set/reset - correct approach.
 *  BR = Bit Reset (set low), BS = Bit Set (set high)
 * ========================================================= */
#define CSN_LOW() (GPIOB->BSRR = GPIO_BSRR_BR2)
#define CSN_HIGH() (GPIOB->BSRR = GPIO_BSRR_BS2)
#define CE_LOW() (GPIOB->BSRR = GPIO_BSRR_BR1)
#define CE_HIGH() (GPIOB->BSRR = GPIO_BSRR_BS1)

extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart1;

/* =========================================================
 *  SPI COMMUNICATION PRIMITIVES
 * ========================================================= */

uint8_t NRF24_ReadReg(uint8_t reg) {
    /* FIX R_REGISTER command = 0x00 | reg (mask top 3 bits to be safe) */
    uint8_t command = NRF_CMD_R_REGISTER | (reg & 0x1F);
    uint8_t res = 0;
    uint8_t dummy = 0xFF;

    CSN_LOW();
    HAL_SPI_Transmit(&hspi1, &command, 1, 1);
    HAL_SPI_TransmitReceive(&hspi1, &dummy, &res, 1, 1);
    CSN_HIGH();
    return res;
}

void NRF24_WriteReg(uint8_t reg, uint8_t data) {
    uint8_t buf[2];
    /* FIX W_REGISTER command = 0x20 | reg (mask reg bits) */
    buf[0] = NRF_CMD_W_REGISTER | (reg & 0x1F);
    buf[1] = data;

    CSN_LOW();
    HAL_SPI_Transmit(&hspi1, buf, 2, 1);
    CSN_HIGH();
}

void NRF24_WriteAddr(uint8_t reg, uint8_t* addr) {
    uint8_t cmd = NRF_CMD_W_REGISTER | (reg & 0x1F);
    CSN_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 1);
    HAL_SPI_Transmit(&hspi1, addr, 5, 1);
    CSN_HIGH();
}

void NRF24_ReadPayload(uint8_t* data) {
    uint8_t cmd = NRF_CMD_R_RX_PAYLOAD;
    CSN_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 1);
    /* FIX use NRF_PAYLOAD_SIZE macro - single source of truth */
    HAL_SPI_Receive(&hspi1, data, NRF_PAYLOAD_SIZE, 10);
    CSN_HIGH();
}

void NRF24_SendACK_Payload() {
    CSN_LOW();
    uint8_t cmd = 0xA8; // W_ACK_PAYLOAD for pipe 0
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 1);
    HAL_SPI_Transmit(&hspi1, (uint8_t*)&current_ack_payload, sizeof(ACK_Payload_t), 1);
    CSN_HIGH();
}

uint8_t NRF24_IsHardwareAlive(void) {
    // Read Address Width register.
    // If unplugged, SPI returns 0x00 or 0xFF.
    uint8_t aw = NRF24_ReadReg(NRF_REG_SETUP_AW);
    return (aw == 0x03);
}

/* =========================================================
 *  EXTI SETUP FOR IRQ PIN (PB0)
 * ========================================================= */
void NRF24_EXTI_Setup(void) {
    /* 1. Enable peripheral clocks */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    /* 2. PB0 as input (MODER bits [1:0] = 00) */
    GPIOB->MODER &= ~(0x3 << (0 * 2));

    /* 3. Route PB0 to EXTI0 via SYSCFG
     * EXTICR[0] controls EXTI0..3, bits[3:0] for EXTI0
     * 0x1 = Port B */
    SYSCFG->EXTICR[0] &= ~(0xF << 0);
    SYSCFG->EXTICR[0] |= (0x1 << 0);

    /* 4. Unmask line 0, falling-edge trigger
     * nRF24 IRQ is active-low, fires on falling edge */
    EXTI->IMR |= (1 << 0);
    EXTI->FTSR |= (1 << 0);

    /* 5. NVIC: set priority and enable
     * FIX Priority 1 (not 0) - leave priority 0 for
     * truly critical IRQs. Using 0 means this ISR can't
     * be preempted by anything, which is usually wrong. */
    NVIC_SetPriority(EXTI0_IRQn, 1);
    NVIC_EnableIRQ(EXTI0_IRQn);
}

/* =========================================================
 *  RECEIVER INITIALIZATION
 * ========================================================= */
void NRF24_Init_Reciever(void) {
    CE_LOW();
    CSN_HIGH();
    HAL_Delay(5); /* Allow nRF24 to settle after power-up */

    /* CONFIG register = 0x0F
     * Bit 3 (EN_CRC)  = 1  -> CRC enabled
     * Bit 2 (CRCO)    = 1  -> 2-byte CRC  (matches Arduino RF24_CRC_16)
     * Bit 1 (PWR_UP)  = 1  -> Powered up
     * Bit 0 (PRIM_RX) = 1  -> Primary Receiver mode
     * Result: 0b00001111 = 0x0F */
    NRF24_WriteReg(NRF_REG_CONFIG, 0x0F);

    /* FIX Explicitly enable Auto-Acknowledgment on PIPE0. */
    NRF24_WriteReg(NRF_REG_EN_AA, 0x01);
    NRF24_WriteReg(NRF_REG_SETUP_RETR, 0x5F); // 15 Retries @ 1500us delay
    NRF24_WriteReg(NRF_REG_FEATURE, 0x06);
    NRF24_WriteReg(NRF_REG_DYNPD, 0x01);

    /* FIX Explicitly enable Pipe 0 in EN_RXADDR.
     * Default reset value is 0x03 (pipe 0+1 enabled), but
     * we're explicit to not rely on reset state after flush/reinit. */
    NRF24_WriteReg(NRF_REG_EN_RXADDR, 0x01);

    /* RF Channel 100 (2.400 + 0.100 = 2.500 GHz)
     * Keep away from WiFi (typically channels 1,6,11 = 2.412/2.437/2.462 GHz) */
    NRF24_WriteReg(NRF_REG_RF_CH, 100);

    /* RF_SETUP = 0x26 = 0b00100110
     * Bits [6:5] RF_DR_LOW,RF_DR_HIGH = 1,0 -> 250 kbps (matches Arduino RF24_250KBPS)
     * Bits [2:1] RF_PWR = 11 -> 0 dBm (max power) */
    NRF24_WriteReg(NRF_REG_RF_SETUP, 0x26);

    /* SETUP_AW: address width
     * 0x03 = 5-byte address (AW[1:0] = 11) */
    NRF24_WriteReg(NRF_REG_SETUP_AW, 0x03);

    /* Set Pipe 0 RX address to match transmitter TX address */
    NRF24_WriteAddr(NRF_REG_RX_ADDR_P0, DroneAddr);

    /* FIX use NRF_PAYLOAD_SIZE macro */
    NRF24_WriteReg(NRF_REG_RX_PW_P0, NRF_PAYLOAD_SIZE);

    /* Clear all status flags (RX_DR, TX_DS, MAX_RT) */
    NRF24_WriteReg(NRF_REG_STATUS, 0x70);

    /* Flush RX FIFO to start clean */
    uint8_t flush = NRF_CMD_FLUSH_RX;
    CSN_LOW();
    HAL_SPI_Transmit(&hspi1, &flush, 1, 1);
    CSN_HIGH();

    /* CE high: enter RX mode and start listening */
    CE_HIGH();
    NRF24_SendACK_Payload();
}

/* =========================================================
 *  TRANSMITTER INITIALIZATION
 * ========================================================= */
void NRF24_Init_Transmitter(void) {
    CE_LOW();
    CSN_HIGH();
    HAL_Delay(5);

    /* CONFIG = 0x0E
     * EN_CRC=1, CRCO=1 (2-byte CRC), PWR_UP=1, PRIM_RX=0 (PTX mode) */
    NRF24_WriteReg(NRF_REG_CONFIG, 0x0E);

    /* FIX Disable Auto-Ack to match receiver config */
    NRF24_WriteReg(NRF_REG_EN_AA, 0x00);

    NRF24_WriteReg(NRF_REG_RF_CH, 100);
    NRF24_WriteReg(NRF_REG_RF_SETUP, 0x26);
    NRF24_WriteReg(NRF_REG_SETUP_AW, 0x03);

    /* Both TX_ADDR and RX_ADDR_P0 must be set to the same address
     * when using Auto-Ack. If AA is disabled, only TX_ADDR matters. */
    NRF24_WriteAddr(NRF_REG_RX_ADDR_P0, DroneAddr);
    NRF24_WriteAddr(NRF_REG_TX_ADDR, DroneAddr);

    /* Flush TX FIFO */
    uint8_t flush = NRF_CMD_FLUSH_TX;
    CSN_LOW();
    HAL_SPI_Transmit(&hspi1, &flush, 1, 1);
    CSN_HIGH();

    NRF24_WriteReg(NRF_REG_STATUS, 0x70);
    HAL_Delay(2);
    /* Note: CE stays LOW in TX standby. Pulse CE to fire a packet. */
}

/* =========================================================
 *  TRANSMIT PACKET
 * ========================================================= */
void NRF24_Send(uint8_t* data) {
    uint8_t cmd = NRF_CMD_W_TX_PAYLOAD;

    NRF24_WriteReg(NRF_REG_STATUS, 0x70); /* Clear flags */

    CSN_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 1);
    HAL_SPI_Transmit(&hspi1, data, NRF_PAYLOAD_SIZE, 1);
    CSN_HIGH();

    /* Pulse CE >= 10us to trigger transmission.
     * HAL_Delay(1) = 1ms, more than enough. */
    CE_HIGH();
    HAL_Delay(1);
    CE_LOW();
}

/* =========================================================
 *  FIX EXTRACTED RX HANDLER
 *  Previously EXTI0_IRQHandler() was called directly from
 *  main() which is unsafe (re-entrant ISR risk + bypasses
 *  NVIC hw stack frame). Extract the logic here so BOTH
 *  the ISR and the polling fallback in main() call this
 *  single function.
 * ========================================================= */
void NRF24_HandleRX(void) {
    // 1. Clear the RX_DR flag IMMEDIATELY to release the IRQ pin
    NRF24_WriteReg(NRF_REG_STATUS, NRF_STATUS_RX_DR);

    uint8_t drone_payload[NRF_PAYLOAD_SIZE];
    NRF24_ReadPayload(drone_payload);

    // 2. Atomic Copy (Critical Section)
    __disable_irq();
    // Using a simple loop or memcpy is safer for struct alignment
    for (int i = 0; i < sizeof(RC_Packet_t); i++) {
        ((uint8_t*)&current_rc_data)[i] = drone_payload[i];
    }
    last_packet_time = HAL_GetTick();
    __enable_irq();

    // 3. FLUSH TX before loading (Crucial for fresh Telemetry)
    // This prevents the FIFO from getting "backed up" with old data
    uint8_t flush_cmd = NRF_CMD_FLUSH_TX;
    CSN_LOW();
    HAL_SPI_Transmit(&hspi1, &flush_cmd, 1, 1);
    CSN_HIGH();

    // 4. Load fresh telemetry for the NEXT packet
    NRF24_SendACK_Payload();
}

/* =========================================================
 *  UART DEBUG PRINT
 * ========================================================= */
void Debug_Print_RC(void) {
    /* FIX Atomic copy of volatile struct before reading fields.
     * Without __disable_irq, ISR could update current_rc_data
     * between reading joy1_x and joy1_y, giving inconsistent data. */
    __disable_irq();
    RC_Packet_t temp = (RC_Packet_t)current_rc_data;
    __enable_irq();

    /* Manual ASCII conversion - no printf, no heap allocation.
     * This is correct for bare-metal UART. */
    uint8_t msg[] = "J1X: 0000 \r";

    // Corrected for 4-digit values (1000-2000)
    msg[5] = (temp.joy1_x / 1000) + '0';
    msg[6] = ((temp.joy1_x / 100) % 10) + '0';
    msg[7] = ((temp.joy1_x / 10) % 10) + '0';
    msg[8] = (temp.joy1_x % 10) + '0';

    HAL_UART_Transmit(&huart1, msg, sizeof(msg) - 1, 10);
}

/* =========================================================
 *  EXTI0 IRQ HANDLER (PB0 - nRF24 IRQ pin)
 *  FIX Now calls NRF24_HandleRX() instead of duplicating logic.
 * ========================================================= */
void EXTI0_IRQHandler(void) {
    if (EXTI->PR & (1 << 0)) {
        uint8_t status = NRF24_ReadReg(NRF_REG_STATUS);

        if (status & NRF_STATUS_RX_DR) {
            NRF24_HandleRX(); /* FIX shared handler - no code duplication */
        }

        if (status & NRF_STATUS_TX_DS) {
            NRF24_WriteReg(NRF_REG_STATUS, NRF_STATUS_TX_DS);
        }

        if (status & NRF_STATUS_MAX_RT) {
            /* MAX_RT: packet failed after max retries.
             * Flush TX FIFO or the module stays stuck. */
            uint8_t flush = NRF_CMD_FLUSH_TX;
            CSN_LOW();
            HAL_SPI_Transmit(&hspi1, &flush, 1, 1);
            CSN_HIGH();
            NRF24_WriteReg(NRF_REG_STATUS, NRF_STATUS_MAX_RT);
        }

        /* Clear EXTI pending bit LAST */
        EXTI->PR |= (1 << 0);
    }
}
