#include "main.h"
#include "nrf24l01.h"

/* Test flags */
static uint8_t spi_test_passed = 0;
static uint8_t nrf24_detected = 0;
uint32_t timeout_RC_PACKETS = pdMS_TO_TICKS(5000);

/* Test task */
void NRF24_TestTask(void* pvParameters) {
    printf("\r\n\n<-[========= NRF24L01 Test Starting =========]->\r\n");

    /* Step 1: Test SPI communication */
    printf("Test 1: SPI Communication... ");
    SPI1_Init();
    init_dwt();

    /* Quick SPI test - read status register */
    CSN_LOW();
    uint8_t status = SPI1_Transfer(NRF_CMD_NOP);
    CSN_HIGH();

    if (status != 0xFF) { /* Any valid response means SPI works */
        spi_test_passed = 1;
        printf("PASSED (status=0x%02X)\r\n", status);
    } else {
        printf("FAILED (status=0x%02X)\r\n", status);
        while (1); /* Stop here if SPI fails */
    }

    /* Step 2: Check if NRF24 is alive */
    printf("Test 2: NRF24 Hardware Detect... ");
    vTaskDelay(pdMS_TO_TICKS(10));

    if (NRF24_IsHardwareAlive()) {
        nrf24_detected = 1;
        printf("PASSED (Setup_AW=0x03)\r\n");
    } else {
        printf("FAILED - Check wiring!\r\n");
        while (1);
    }

    /* Step 3: Initialize Receiver */
    printf("Test 3: Initializing Receiver... ");
    NRF24_Init_Receiver();
    printf("DONE\r\n");

    /* Step 4: Print configuration */
    printf("\r\n<-[=========== NRF24 Configuration ==========]->\r\n");
    printf("CONFIG:     0x%02X\r\n", NRF24_ReadReg(NRF_REG_CONFIG));
    printf("RF_CH:      0x%02X (Channel %d)\r\n", NRF24_ReadReg(NRF_REG_RF_CH), NRF24_ReadReg(NRF_REG_RF_CH));
    printf("RF_SETUP:   0x%02X\r\n", NRF24_ReadReg(NRF_REG_RF_SETUP));
    printf("EN_AA:      0x%02X\r\n", NRF24_ReadReg(NRF_REG_EN_AA));
    printf("FEATURE:    0x%02X\r\n", NRF24_ReadReg(NRF_REG_FEATURE));
    printf("STATUS:     0x%02X\r\n", NRF24_ReadReg(NRF_REG_STATUS));

    NRF24_SendInitialACK_Payload();

    /* Step 5: Wait for packets */
    printf("\r\n<-[========= Waiting for RC packets =========]->\r\n");
    printf("Turn on your transmitter now!\r\n");

    while (true) {
        /* Process any pending NRF24 events */
        NRF24_Task();

        /* Check if we received data */
        if (isDataReady) {
            isDataReady = 0;
            printf("\rRC ID: %d ", rc_packet.packet_id);
            ack_packet.packet_id = rc_packet.packet_id;
        }

        /* Check for timeout (no packets for 2 seconds) */
        if (xTaskGetTickCount() - last_packet_time > timeout_RC_PACKETS) {
            if (!NRF24_IsHardwareAlive()) {
                // TODO: Handle it gracefully {Reduce Throttle & Stuff}
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void init_adc() {
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    GPIOA->MODER |= (0b11 << GPIO_MODER_MODER0_Pos);

    ADC1->CR1 |= (0b01 << ADC_CR1_DISCEN_Pos) | (0b01 << ADC_CR1_EOCIE_Pos);
    ADC1->CR2 |= (0b01 << ADC_CR2_ADON_Pos);
    ADC1->SQR3 |= (18U << ADC_SQR3_SQ1_Pos);
    ADC->CCR |= (0b01 << ADC_CCR_TSVREFE_Pos);

    NVIC_EnableIRQ(ADC_IRQn);
}
uint32_t temp = 0;
extern "C" void ADC_IRQHandler() {
    if (ADC1->SR & ADC_SR_EOC) {
        temp = (uint32_t)ADC1->DR;
        GPIOC->ODR ^= GPIO_ODR_OD13;
    }
}

/* In main() */
int main(void) {
    SystemClock_Config();
    // Initialize GPIO PC13 (Onboard LED)
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    GPIOC->MODER &= ~(3U << (13 * 2));
    GPIOC->MODER |= (1U << (13 * 2));

    GPIOC->ODR |= GPIO_ODR_OD13;
    init_uart(USART1, 115200, 100000000);

    GPIOC->ODR &= ~(GPIO_ODR_OD13_Msk);
    // printf("Test 3: Initializing Receiver... ");

    /* Create test task */
    xTaskCreate(NRF24_TestTask, "NRF24Test", 512, NULL, 3, NULL);

    /* Start scheduler */
    vTaskStartScheduler();

    while (1) {
    }
}
