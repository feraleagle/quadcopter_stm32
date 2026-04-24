/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    main.c
 * @brief   nRF24L01 RC Receiver - STM32F411 Black Pill
 *          Fixes applied:
 *          1. SPI prescaler: /2 (50MHz) -> /16 (6.25MHz), nRF24 max = 10MHz
 *          2. Removed manual EXTI0_IRQHandler() call; use NRF24_HandleRX()
 *          3. Removed dead I2C init code
 ******************************************************************************
 */
/* USER CODE END Header */

#include "main.h"
#include "cmsis_gcc.h"
#include "nrf24l01.h"
#include "stm32f411xe.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_spi.h"
#include "stm32f4xx_hal_uart.h"

/* =========================================================
 *  PERIPHERAL HANDLES
 *  FIX: Removed hi2c1 - I2C is not used in this project.
 *  Keeping unused HAL handles wastes ~80 bytes of RAM each
 *  and implies peripherals are initialized when they aren't.
 * ========================================================= */
SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart1;

/* =========================================================
 *  FUNCTION PROTOTYPES
 * ========================================================= */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);

/* =========================================================
 *  MAIN
 * ========================================================= */
int main(void) {
    /* 1. MCU Initialization */
    HAL_Init();
    SystemClock_Config();

    /* 2. Peripheral Init
     * FIX: Removed MX_I2C1_Init() - I2C not needed here */
    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_USART1_UART_Init();

    /* 3. Radio Power-Up
     * nRF24 needs 100ms from VCC stable to begin SPI comms */
    HAL_Delay(100);

    NRF24_EXTI_Setup();
    NRF24_Init_Reciever();

    /* 4. SPI Sanity Check
     * Read CONFIG register (0x00). After NRF24_Init_Reciever()
     * wrote 0x0F, reading it back confirms SPI is working.
     * 0x00 or 0xFF = SPI wiring problem (MISO floating or MOSI shorted) */
    uint8_t config_verify = NRF24_ReadReg(NRF_REG_CONFIG);
    if (config_verify == 0x0F) {
        uint8_t ok_msg[] = ">>> NRF24 SUCCESS: Radio configured. CONFIG=0x0F\r\n";
        HAL_UART_Transmit(&huart1, ok_msg, sizeof(ok_msg) - 1, 100);
    } else {
        uint8_t err_msg[] = "!!! NRF24 ERROR: SPI failed. Check wiring & prescaler.\r\n";
        HAL_UART_Transmit(&huart1, err_msg, sizeof(err_msg) - 1, 100);
        /* Optionally halt here during bring-up: */
        /* while(1); */
    }

    /* 5. Loop Variables */
    uint32_t last_display_time = 0;
    uint32_t last_heartbeat = 0;

    /* 6. Main Loop (Interrupt Test Version) */
    while (1) {

        /* A. POLLING FALLBACK - DISABLED FOR TESTING
        uint8_t status = NRF24_ReadReg(NRF_REG_STATUS);
        if (status & NRF_STATUS_RX_DR) {
            NRF24_HandleRX();
        }
        */

        /* B. FAILSAFE - REMOVED AS REQUESTED
           (Data will now simply persist in the struct until a new packet arrives) */

        /* C. TIMED TELEMETRY */
        if (HAL_GetTick() - last_display_time > 100) {
            // Just print the data. If values change when you move the Arduino stick,
            // INTERRUPTS ARE WORKING!
            Debug_Print_RC();
            last_display_time = HAL_GetTick();
        }

        /* D. HEARTBEAT LED */
        if (HAL_GetTick() - last_heartbeat > 1000) {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            last_heartbeat = HAL_GetTick();
        }
    }
}

/* =========================================================
 *  SYSTEM CLOCK CONFIGURATION
 *  HSE=25MHz, PLL: M=25, N=200, P=2 -> SYSCLK=100MHz
 *  APB1 = 50MHz (HCLK/2), APB2 = 100MHz (HCLK/1)
 *  SPI1 is on APB2. With prescaler /16: 100/16 = 6.25MHz.
 *  nRF24L01 max SPI clock = 10MHz. 6.25MHz is safe.
 * ========================================================= */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;
    RCC_OscInitStruct.PLL.PLLN = 200;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) {
        Error_Handler();
    }

    HAL_RCC_EnableCSS();
}

/* =========================================================
 *  SPI1 INITIALIZATION
 *  FIX: BaudRatePrescaler changed from _2 to _16
 *
 *  APB2 clock = 100MHz
 *  _2  -> 100/2  = 50.0 MHz  !! EXCEEDS nRF24 10MHz max !!
 *  _4  -> 100/4  = 25.0 MHz  !! Still too fast !!
 *  _8  -> 100/8  = 12.5 MHz  !! Slightly over limit !!
 *  _16 -> 100/16 =  6.25 MHz  OK, safe margin below 10MHz
 *  _32 -> 100/32 =  3.12 MHz  Also fine, more conservative
 *
 *  Mode 0 (CPOL=0, CPHA=0) matches nRF24L01 SPI spec.
 * ========================================================= */
static void MX_SPI1_Init(void) {
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;               /* CPOL=0 */
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;                   /* CPHA=0 -> Mode 0 */
    hspi1.Init.NSS = SPI_NSS_SOFT;                           /* Software CS, we toggle manually */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16; /* FIX: was _2 (50MHz -> 6.25MHz) */
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE; /* nRF24 uses its own CRC */
    hspi1.Init.CRCPolynomial = 10;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        Error_Handler();
    }
}

/* =========================================================
 *  USART1 INITIALIZATION
 *  115200 8N1, TX+RX, no hardware flow control
 * ========================================================= */
static void MX_USART1_UART_Init(void) {
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}

/* =========================================================
 *  GPIO INITIALIZATION
 *  PB1 = CE  (nRF24 Chip Enable)
 *  PB2 = CSN (nRF24 Chip Select, active low)
 *  PA5 = SCK, PA6 = MISO, PA7 = MOSI (SPI1, configured by HAL_SPI_Init)
 *  PB0 = IRQ (configured as EXTI input in NRF24_EXTI_Setup)
 *  PC13 = Onboard LED (Black Pill)
 * ========================================================= */
static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable GPIO clocks */
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE(); /* ADD: needed for PC13 LED */

    /* Set CE and CSN high (inactive) before configuring as outputs */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1 | GPIO_PIN_2, GPIO_PIN_SET);

    /* PB1 (CE), PB2 (CSN) as push-pull outputs */
    GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PC13 as output for heartbeat LED
     * Black Pill LED is active-low (LED on when PC13 = 0) */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET); /* Start with LED off */
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

/* =========================================================
 *  ERROR HANDLER
 * ========================================================= */
void Error_Handler(void) {
    __disable_irq();
    while (1) {
        /* Spin forever on hard fault or HAL error.
         * In debug: set a breakpoint here.
         * In production: add a watchdog reset. */
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line) {
    /* Optionally: log file+line over UART before hanging */
}
#endif
