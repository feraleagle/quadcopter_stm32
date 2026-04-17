/******************************************************************************
  Function: Initialize USART2
******************************************************************************/
void init_uart() {
    // Enable Clocks USART2 & GPIOA
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    // Clear & Set Mode to Alternate Function
    GPIOA->MODER &= ~GPIO_MODER_MODER2_Msk;
    GPIOA->MODER |= (0b10 << GPIO_MODER_MODER2_Pos);

    // Clear & Set AF7 to PA2
    GPIOA->AFR[0] &= ~GPIO_AFRL_AFSEL2_Msk;
    GPIOA->AFR[0] |= (7U << GPIO_AFRL_AFSEL2_Pos);

    // Set Baud Rate for HSI 16 Mhz 115200 Baud Rate
    // BRR = 16,000,000 / (16 * 115200) -> 8.680
    // Mantissa = 8  |  Fraction = 0.680 * 16 -> 11
    USART2->BRR = (8U << USART_BRR_DIV_Mantissa_Pos) | (11U << USART_BRR_DIV_Fraction_Pos);
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE; // Enable Transmission & UART
}

/******************************************************************************
  Function: Send 1 Byte
******************************************************************************/
void sendByte(char byte) {
    while (!(USART2->SR & USART_SR_TXE)); // Wait till TXE
    USART2->DR = byte;                    // Send Byte
}

/******************************************************************************
  Function: Send 10 Bytes Array
******************************************************************************/
void sendData(char* data, int& size) {
    for (int i = 0; i < size; i++) {
        if (data[i] == '\0')
            break; // Don't send null terminator
        sendByte(data[i]);
    }
}
