#ifndef USART_H
#define USART_H

#include "main.h"

/* =========================================================
 *  FUNCTION PROTOTYPES
 * ========================================================= */
void init_uart(USART_TypeDef* USART, const int baud, const int clock);
#endif
