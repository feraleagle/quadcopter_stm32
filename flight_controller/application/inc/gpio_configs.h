#ifndef GPIO_CONFIGS_H
#define GPIO_CONFIGS_H

#include "main.h"

/* =========================================================
 *  GPIO Mappings
 * ========================================================= */
/* @SPI  :    PA5(SCK), PA6(MISO), PA7(MOSI),
 *            PB1(CE), PB2(CSN);
 * @USART:    PA9(TX), PA10(RX);
 */

/* =========================================================
 *  FUNCTION PROTOTYPES
 * ========================================================= */
void init_builtin_led();
#endif
