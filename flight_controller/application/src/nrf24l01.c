#include "nrf24l01.h"

/* =========================================================
 * SHARED VARIABLE DEFINITIONS
 * ========================================================= */
volatile RC_Packet_t rc_packet;
volatile ACK_Packet_t ack_packet;

volatile uint8_t isDataReady = 0;
volatile uint32_t last_packet_time = 0;

uint8_t DroneAddr[] = {0xE7, 0xE7, 0xFF, 0x11, 0xE2};

/* =========================================================
 * HARDWARE MACROS
 * ========================================================= */
#define CSN_LOW() (GPIOB->BSRR = GPIO_BSRR_BR2)
#define CSN_HIGH() (GPIOB->BSRR = GPIO_BSRR_BS2)
#define CE_LOW() (GPIOB->BSRR = GPIO_BSRR_BR1)
#define CE_HIGH() (GPIOB->BSRR = GPIO_BSRR_BS1)

/* =========================================================
 * SIMPLE WORKING SPI TRANSFER WITH TIMEOUT OF 04 MICROSECs
 * ========================================================= */
uint8_t SPI1_Transfer(uint8_t tx_byte) {
    uint32_t start = DWT->CYCCNT;
    uint32_t micro_seconds = (SystemCoreClock / 1000000);
    while (!(SPI1->SR & SPI_SR_TXE) && (DWT->CYCCNT - start) < 1 * micro_seconds);
    SPI1->DR = tx_byte;
    start = DWT->CYCCNT;
    while (!(SPI1->SR & SPI_SR_RXNE) && (DWT->CYCCNT - start) < 3 * micro_seconds);
    return SPI1->DR;
}

/* =========================================================
 * INITIALIZATION
 * ========================================================= */
void SPI1_Init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;

    /* Configure SPI pins: PA5(SCK), PA6(MISO), PA7(MOSI) */
    GPIOA->MODER &= ~(GPIO_MODER_MODER5 | GPIO_MODER_MODER6 | GPIO_MODER_MODER7);
    GPIOA->MODER |= (2 << GPIO_MODER_MODER5_Pos) |
                    (2 << GPIO_MODER_MODER6_Pos) |
                    (2 << GPIO_MODER_MODER7_Pos);

    GPIOA->AFR[0] |= (5 << (5 * 4)) | /* AF5 for PA5 */
                     (5 << (6 * 4)) | /* AF5 for PA6 */
                     (5 << (7 * 4));  /* AF5 for PA7 */

    GPIOA->OSPEEDR |= (3 << GPIO_OSPEEDR_OSPEED5_Pos) |
                      (3 << GPIO_OSPEEDR_OSPEED6_Pos) |
                      (3 << GPIO_OSPEEDR_OSPEED7_Pos);

    /* Configure CE and CSN pins (PB1, PB2) */
    GPIOB->MODER |= (1 << (1 * 2)) | (1 << (2 * 2));
    GPIOB->OTYPER &= ~(GPIO_OTYPER_OT1 | GPIO_OTYPER_OT2);
    GPIOB->OSPEEDR |= (3 << (1 * 2)) | (3 << (2 * 2));

    /* Configure SPI - 6.25MHz (100MHz / 16) */
    SPI1->CR1 = SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_MSTR | (3 << SPI_CR1_BR_Pos) | SPI_CR1_SPE;
}

void init_dwt(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* =========================================================
 * NRF24 FUNCTIONS
 * ========================================================= */
uint8_t NRF24_ReadReg(uint8_t reg) {
    uint8_t res;
    CSN_LOW();
    SPI1_Transfer(NRF_CMD_R_REGISTER | (reg & 0x1F));
    res = SPI1_Transfer(0xFF);
    CSN_HIGH();
    return res;
}

void NRF24_WriteReg(uint8_t reg, uint8_t data) {
    CSN_LOW();
    SPI1_Transfer(NRF_CMD_W_REGISTER | (reg & 0x1F));
    SPI1_Transfer(data);
    CSN_HIGH();
}

void NRF24_WriteAddr(uint8_t reg, uint8_t* addr) {
    CSN_LOW();
    SPI1_Transfer(NRF_CMD_W_REGISTER | (reg & 0x1F));
    for (int i = 0; i < 5; i++) SPI1_Transfer(addr[i]);
    CSN_HIGH();
}

void NRF24_ReadPayload(uint8_t* data) {
    CSN_LOW();
    SPI1_Transfer(NRF_CMD_R_RX_PAYLOAD);
    for (int i = 0; i < NRF_PAYLOAD_SIZE; i++) data[i] = SPI1_Transfer(0xFF);
    CSN_HIGH();
}

void NRF24_SendACK_Payload(void) {
    CSN_LOW();
    SPI1_Transfer(0xA8); /* W_ACK_PAYLOAD pipe 0 */
    uint8_t* p = (uint8_t*)&ack_packet;
    for (unsigned int i = 0; i < sizeof(ACK_Packet_t); i++) SPI1_Transfer(p[i]);
    CSN_HIGH();
}

void NRF24_SendInitialACK_Payload(void) {
    char handShake[38] = "The Eagle Has Connected Successfully!";
    CSN_LOW();
    SPI1_Transfer(0xA8); /* W_ACK_PAYLOAD pipe 0 */
    uint8_t* p = (uint8_t*)&handShake;
    for (unsigned int i = 0; i < sizeof(handShake); i++) SPI1_Transfer(p[i]);
    CSN_HIGH();
}

uint8_t NRF24_IsHardwareAlive(void) {
    return (NRF24_ReadReg(NRF_REG_SETUP_AW) == 0x03);
}

void NRF24_Init_Receiver(void) {
    init_dwt();
    CE_LOW();
    CSN_HIGH();

    NRF24_WriteReg(NRF_REG_CONFIG, 0x0F);
    NRF24_WriteReg(NRF_REG_EN_AA, 0x01);
    NRF24_WriteReg(NRF_REG_SETUP_RETR, 0x5F);
    NRF24_WriteReg(NRF_REG_FEATURE, 0x06);
    NRF24_WriteReg(NRF_REG_DYNPD, 0x01);
    NRF24_WriteReg(NRF_REG_EN_RXADDR, 0x01);
    NRF24_WriteReg(NRF_REG_RF_CH, 100);
    NRF24_WriteReg(NRF_REG_RF_SETUP, 0x26);
    NRF24_WriteReg(NRF_REG_SETUP_AW, 0x03);
    NRF24_WriteReg(NRF_REG_STATUS, 0x70);

    NRF24_WriteAddr(NRF_REG_RX_ADDR_P0, DroneAddr);
    NRF24_WriteReg(NRF_REG_RX_PW_P0, NRF_PAYLOAD_SIZE);

    CSN_LOW();
    SPI1_Transfer(NRF_CMD_FLUSH_RX);
    SPI1_Transfer(NRF_CMD_FLUSH_TX);
    CSN_HIGH();

    CE_HIGH();
}

/* =========================================================
 * RX HANDLER - Now runs in task context, NOT ISR!
 * ========================================================= */
void NRF24_ProcessRX(void) {
    uint8_t drone_payload[NRF_PAYLOAD_SIZE];

    /* Clear IRQ flag */
    NRF24_WriteReg(NRF_REG_STATUS, NRF_STATUS_RX_DR);

    /* Read payload */
    NRF24_ReadPayload(drone_payload);

    /* Copy to shared variable - using task context critical section */
    taskENTER_CRITICAL();

    memcpy((void*)&rc_packet, drone_payload, sizeof(RC_Packet_t));
    last_packet_time = xTaskGetTickCount(); /* Now safe - in task context */
    isDataReady = 1;

    taskEXIT_CRITICAL();

    /* Prepare for next packet */
    CSN_LOW();
    SPI1_Transfer(NRF_CMD_FLUSH_TX);
    CSN_HIGH();

    /* Send ACK with telemetry */
    NRF24_SendACK_Payload();
}

/* =========================================================
 * Call this from your main loop or a low-priority task
 * ========================================================= */
void NRF24_Task(void) {

    /* Check status - quick SPI read (still in task, but OK) */

    uint8_t status = NRF24_ReadReg(NRF_REG_STATUS);

    if (status & NRF_STATUS_RX_DR) {
        NRF24_ProcessRX();
    }

    if (status & NRF_STATUS_TX_DS) {
        NRF24_WriteReg(NRF_REG_STATUS, NRF_STATUS_TX_DS);
    }

    if (status & NRF_STATUS_MAX_RT) {
        CSN_LOW();
        SPI1_Transfer(NRF_CMD_FLUSH_TX);
        CSN_HIGH();
        NRF24_WriteReg(NRF_REG_STATUS, NRF_STATUS_MAX_RT);
    }
}
