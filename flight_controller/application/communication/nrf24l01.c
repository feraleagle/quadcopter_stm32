// #include "nrf24l01.h"
// #include "main.h"
// #include <stdint.h>
//
// /* =========================================================
//  * SHARED VARIABLE DEFINITIONS
//  * ========================================================= */
// volatile RC_Packet_t current_rc_data;
// volatile uint8_t isDataReady = 0;
// volatile ACK_Payload_t current_ack_payload;
// volatile uint32_t last_packet_time = 0;
//
// uint8_t DroneAddr[] = {0xE7, 0xE7, 0xFF, 0x11, 0xE2};
//
// /* =========================================================
//  * HARDWARE MACROS
//  * ========================================================= */
// #define CSN_LOW() (GPIOB->BSRR = GPIO_BSRR_BR2)
// #define CSN_HIGH() (GPIOB->BSRR = GPIO_BSRR_BS2)
// #define CE_LOW() (GPIOB->BSRR = GPIO_BSRR_BR1)
// #define CE_HIGH() (GPIOB->BSRR = GPIO_BSRR_BS1)
//
// /* =========================================================
//  * DMA2 STREAM0 — SPI1 RX
//  * ========================================================= */
// void init_dma_spi1(void) {
//     RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
//
//     DMA2_Stream0->CR &= ~DMA_SxCR_EN;
//     while (DMA2_Stream0->CR & DMA_SxCR_EN); /* drain */
//
//     DMA2->LIFCR = 0x3F; /* clear Stream0 flags */
//
//     DMA2_Stream0->CR =
//         (3UL << DMA_SxCR_CHSEL_Pos) | /* CH3 = SPI1_RX    */
//         (2UL << DMA_SxCR_PL_Pos) |    /* priority high    */
//         DMA_SxCR_MINC |               /* memory increment  */
//         DMA_SxCR_TCIE;
//     /* TC interrupt enable */
//     /* DIR=00 → periph-to-memory (default), MSIZE/PSIZE=00 (8-bit) */
//
//     DMA2_Stream0->NDTR = NRF_PAYLOAD_SIZE;
//     DMA2_Stream0->PAR = (uintptr_t)&SPI1->DR;
//     DMA2_Stream0->M0AR = (uintptr_t)&current_rc_data;
//
//     NVIC_SetPriority(DMA2_Stream0_IRQn, 2);
//     NVIC_EnableIRQ(DMA2_Stream0_IRQn);
// }
//
// void enable_dma_spi1(void) {
//     /* Re-arm NDTR before each enable — mandatory after stream completes */
//     DMA2_Stream0->NDTR = NRF_PAYLOAD_SIZE;
//     DMA2_Stream0->CR |= DMA_SxCR_EN;
//     /* Also enable SPI1 RX DMA request */
//     SPI1->CR2 |= SPI_CR2_RXDMAEN;
// }
//
// void DMA2_Stream0_IRQHandler(void) {
//     if (DMA2->LISR & DMA_LISR_TCIF0) {
//         DMA2->LIFCR |= DMA_LIFCR_CTCIF0; /* clear TC flag */
//         SPI1->CR2 &= ~SPI_CR2_RXDMAEN;
//         /* stop DMA request */
//         isDataReady = 1;
//     }
// }
//
// /* =========================================================
//  * SPI COMMUNICATION PRIMITIVES
//  * ========================================================= */
//
// uint8_t NRF24_ReadReg(uint8_t reg) {
//     uint8_t res;
//     CSN_LOW();
//     SPI1_Transfer(NRF_CMD_R_REGISTER | (reg & 0x1F)); /* send command, discard status */
//     res = SPI1_Transfer(0xFF);
//     /* clock in data byte */
//     CSN_HIGH();
//     return res;
// }
//
// void NRF24_WriteReg(uint8_t reg, uint8_t data) {
//     CSN_LOW();
//     SPI1_Transfer(NRF_CMD_W_REGISTER | (reg & 0x1F));
//     SPI1_Transfer(data);
//     CSN_HIGH();
// }
//
// void NRF24_WriteAddr(uint8_t reg, uint8_t* addr) {
//     CSN_LOW();
//     SPI1_Transfer(NRF_CMD_W_REGISTER | (reg & 0x1F));
//     for (int i = 0; i < 5; i++) SPI1_Transfer(addr[i]);
//     CSN_HIGH();
// }
//
// void NRF24_ReadPayload(uint8_t* data) {
//     CSN_LOW();
//     SPI1_Transfer(NRF_CMD_R_RX_PAYLOAD);
//     for (int i = 0; i < NRF_PAYLOAD_SIZE; i++)
//         data[i] = SPI1_Transfer(0xFF);
//     CSN_HIGH();
// }
//
// void NRF24_SendACK_Payload(void) {
//     CSN_LOW();
//     SPI1_Transfer(0xA8); /* W_ACK_PAYLOAD pipe 0 */
//     uint8_t* p = (uint8_t*)&current_ack_payload;
//     for (int i = 0; i < (int)sizeof(ACK_Payload_t); i++)
//         SPI1_Transfer(p[i]);
//     CSN_HIGH();
// }
//
// uint8_t NRF24_IsHardwareAlive(void) {
//     uint8_t aw = NRF24_ReadReg(NRF_REG_SETUP_AW);
//     return (aw == 0x03);
// }
//
// /* =========================================================
//  * EXTI0 SETUP FOR nRF24 IRQ PIN (PB0)
//  * ========================================================= */
// void NRF24_EXTI_Setup(void) {
//     RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
//     RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
//
//     GPIOB->MODER &= ~(0x3 << (0 * 2)); /* PB0 input */
//
//     SYSCFG->EXTICR[0] &= ~(0xF << 0);
//     SYSCFG->EXTICR[0] |= (0x1 << 0);
//     /* PB → EXTI0 */
//
//     EXTI->IMR |= (1 << 0);
//     /* unmask line 0 */
//     EXTI->FTSR |= (1 << 0);
//     /* falling edge (IRQ active-low) */
//
//     NVIC_SetPriority(EXTI0_IRQn, 1);
//     NVIC_EnableIRQ(EXTI0_IRQn);
// }
//
// /* =========================================================
//  * RECEIVER INIT
//  * ========================================================= */
// void NRF24_Init_Reciever(void) {
//     CE_LOW();
//     CSN_HIGH();
//
//     NRF24_WriteReg(NRF_REG_CONFIG, 0x0F);     /* EN_CRC|CRCO|PWR_UP|PRIM_RX */
//     NRF24_WriteReg(NRF_REG_EN_AA, 0x01);      /* Auto-Ack pipe 0 */
//     NRF24_WriteReg(NRF_REG_SETUP_RETR, 0x5F); /* 15 retries, 1500 µs */
//     NRF24_WriteReg(NRF_REG_FEATURE, 0x06);    /* EN_ACK_PAY | EN_DYN_ACK */
//     NRF24_WriteReg(NRF_REG_DYNPD, 0x01);      /* Dynamic payload pipe 0 */
//     NRF24_WriteReg(NRF_REG_EN_RXADDR, 0x01);  /* Enable pipe 0 */
//     NRF24_WriteReg(NRF_REG_RF_CH, 100);
//     NRF24_WriteReg(NRF_REG_RF_SETUP, 0x26); /* 250 kbps, 0 dBm */
//     NRF24_WriteReg(NRF_REG_SETUP_AW, 0x03); /* 5-byte address */
//     NRF24_WriteAddr(NRF_REG_RX_ADDR_P0, DroneAddr);
//     NRF24_WriteReg(NRF_REG_RX_PW_P0, NRF_PAYLOAD_SIZE);
//     NRF24_WriteReg(NRF_REG_STATUS, 0x70); /* Clear RX_DR|TX_DS|MAX_RT */
//
//     /* Flush RX FIFO */
//     CSN_LOW();
//     SPI1_Transfer(NRF_CMD_FLUSH_RX);
//     CSN_HIGH();
//
//     CE_HIGH();
//     NRF24_SendACK_Payload();
// }
//
// /* =========================================================
//  * TRANSMITTER INIT
//  * ========================================================= */
// void NRF24_Init_Transmitter(void) {
//     CE_LOW();
//     CSN_HIGH();
//
//     NRF24_WriteReg(NRF_REG_CONFIG, 0x0E); /* EN_CRC|CRCO|PWR_UP, PRIM_RX=0 */
//     NRF24_WriteReg(NRF_REG_EN_AA, 0x00);  /* Disable Auto-Ack (PTX only) */
//     NRF24_WriteReg(NRF_REG_RF_CH, 100);
//     NRF24_WriteReg(NRF_REG_RF_SETUP, 0x26);
//     NRF24_WriteReg(NRF_REG_SETUP_AW, 0x03);
//     NRF24_WriteAddr(NRF_REG_RX_ADDR_P0, DroneAddr);
//     NRF24_WriteAddr(NRF_REG_TX_ADDR, DroneAddr);
//
//     /* Flush TX FIFO */
//     CSN_LOW();
//     SPI1_Transfer(NRF_CMD_FLUSH_TX);
//     CSN_HIGH();
//
//     NRF24_WriteReg(NRF_REG_STATUS, 0x70);
// }
//
// /* =========================================================
//  * TRANSMIT PACKET
//  * ========================================================= */
// void NRF24_Send(uint8_t* data) {
//     NRF24_WriteReg(NRF_REG_STATUS, 0x70);
//
//     CSN_LOW();
//     SPI1_Transfer(NRF_CMD_W_TX_PAYLOAD);
//     for (int i = 0; i < NRF_PAYLOAD_SIZE; i++) SPI1_Transfer(data[i]);
//     CSN_HIGH();
//
//     /* CE pulse ≥ 10 µs to fire TX.
//      * Using DWT for precision — Delay_ms(1) wastes 990 µs. */
//     CE_HIGH();
//     Delay_us(15); /* 15 µs — safe margin above 10 µs minimum */
//     CE_LOW();
// }
//
// /* =========================================================
//  * RX HANDLER — called from EXTI0_IRQHandler
//  * ========================================================= */
// void NRF24_HandleRX(void) {
//     /* 1. Clear RX_DR immediately to de-assert IRQ pin */
//     NRF24_WriteReg(NRF_REG_STATUS, NRF_STATUS_RX_DR);
//
//     uint8_t drone_payload[NRF_PAYLOAD_SIZE];
//     NRF24_ReadPayload(drone_payload);
//
//     /* 2. Atomic copy into shared struct */
//     __disable_irq();
//     for (int i = 0; i < (int)sizeof(RC_Packet_t); i++)
//         ((uint8_t*)&current_rc_data)[i] = drone_payload[i];
//     last_packet_time = HAL_GetTick(); /* bare-metal tick, replaces HAL_GetTick() */
//     __enable_irq();
//
//     /* 3. Flush TX FIFO before loading new ACK payload */
//     CSN_LOW();
//     SPI1_Transfer(NRF_CMD_FLUSH_TX);
//     CSN_HIGH();
//
//     /* 4. Load fresh telemetry for next ACK */
//     NRF24_SendACK_Payload();
// }
//
// /* =========================================================
//  * UART DEBUG PRINT
//  * Manual ASCII — no printf, no heap. Same logic as original.
//  * ========================================================= */
// void Debug_Print_RC(void) {
//     __disable_irq();
//     RC_Packet_t temp = (RC_Packet_t)current_rc_data;
//     __enable_irq();
//
//     uint8_t msg[] = "J1X: 0000 \r";
//     msg[5] = (temp.joy1_x / 1000) + '0';
//     msg[6] = ((temp.joy1_x / 100) % 10) + '0';
//     msg[7] = ((temp.joy1_x / 10) % 10) + '0';
//     msg[8] = (temp.joy1_x % 10) + '0';
// }
//
// /* =========================================================
//  * EXTI0 IRQ HANDLER (PB0 — nRF24 IRQ pin)
//  * ========================================================= */
// void EXTI0_IRQHandler(void) {
//     if (EXTI->PR & (1 << 0)) {
//         uint8_t status = NRF24_ReadReg(NRF_REG_STATUS);
//
//         if (status & NRF_STATUS_RX_DR) NRF24_HandleRX();
//
//         if (status & NRF_STATUS_TX_DS)
//             NRF24_WriteReg(NRF_REG_STATUS, NRF_STATUS_TX_DS);
//
//         if (status & NRF_STATUS_MAX_RT) {
//             CSN_LOW();
//             SPI1_Transfer(NRF_CMD_FLUSH_TX);
//             CSN_HIGH();
//             NRF24_WriteReg(NRF_REG_STATUS, NRF_STATUS_MAX_RT);
//         }
//
//         EXTI->PR |= (1 << 0); /* clear pending bit — always last */
//     }
// }
