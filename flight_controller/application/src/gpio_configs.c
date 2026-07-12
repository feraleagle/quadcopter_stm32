#include "main.h"

/* =========================================================
 *  FUNCTION: Initialize Build-in LED
 * ========================================================= */
void init_builtin_led() {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    GPIOC->MODER &= ~(3U << (13 * 2));
    GPIOC->MODER |= (1U << (13 * 2));

    GPIOC->ODR |= GPIO_ODR_OD13; // Active Low
}
