#include "usart.h"

/* =========================================================
 *  FUNCTION: Initialize USART
 * ========================================================= */
void init_uart(USART_TypeDef* USART, const int baud, const int clock) {
    // 1. Clock Enable & Pin Config
    if (USART == USART2) {
        RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
        RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
        GPIOA->MODER |= (0b10 << GPIO_MODER_MODER2_Pos) | (0b10 << GPIO_MODER_MODER3_Pos);
        GPIOA->AFR[0] |= (7U << GPIO_AFRL_AFSEL2_Pos) | (7U << GPIO_AFRL_AFSEL3_Pos);
    } else if (USART == USART1) {
        RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
        RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
        GPIOA->MODER |= (0b10 << GPIO_MODER_MODER9_Pos) | (0b10 << GPIO_MODER_MODER10_Pos);
        GPIOA->AFR[1] |= (7U << GPIO_AFRH_AFSEL9_Pos) | (7U << GPIO_AFRH_AFSEL10_Pos);
    } else if (USART == USART6) {
        RCC->APB2ENR |= RCC_APB2ENR_USART6EN;
        RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
        GPIOA->MODER |= (0b10 << GPIO_MODER_MODER11_Pos) | (0b10 << GPIO_MODER_MODER12_Pos);
        GPIOA->AFR[1] |= (8U << GPIO_AFRH_AFSEL11_Pos) | (8U << GPIO_AFRH_AFSEL12_Pos);
    }

    // 2. BAUD RATE
    double brr_calc = (double)clock / (16.0 * baud);
    int mantissa = brr_calc;
    int fraction = (brr_calc - mantissa) * 16.0 + 0.5;

    if (fraction >= 16) {
        mantissa += 1;
        fraction = 0;
    }

    USART->BRR = (mantissa << 4) | (fraction & 0x0F);

    // 3. Enable Bits [Transmission, Reception & Enable Peripheral]
    USART->CR1 |= USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    setvbuf(stdout, NULL, _IONBF, 0);
}

void sendByte(USART_TypeDef* USART, char byte) {
    while (!(USART->SR & USART_SR_TXE));
    USART->DR = byte;
}

int _write(int file, char* ptr, int len) {
    for (int i = 0; i < len; i++) {
        sendByte(USART1, ptr[i]);
    }
    return len;
}
