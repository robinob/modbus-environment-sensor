/**
 * @file    board_io.c
 * @brief   DIP switches, LED and the millisecond clock.
 *
 * board_read_address() is still a STUB, filled in by step 5.
 */
#include "board_io.h"

#include "main.h" /* the HAL, and the DIP_x / LED_x pin names */

uint8_t board_read_address(void)
{
    /* Step 5: read DIP_1..DIP_4 (PA0..PA3). A switch turned ON reads 0
     * (pull-up, switch to GND), so the address will be the inverted
     * switch bits + 1, giving 1..16. Until then: always address 1. */
    return 1u;
}

uint32_t board_millis(void)
{
    return HAL_GetTick();
}
