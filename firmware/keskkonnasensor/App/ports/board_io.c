/**
 * @file    board_io.c
 * @brief   DIP switches and LED. STUB: filled in by step 5.
 */
#include "board_io.h"

uint8_t board_read_address(void)
{
    /* Step 5: read DIP_1..DIP_4 (PA0..PA3). A switch turned ON reads 0
     * (pull-up, switch to GND), so the address will be the inverted
     * switch bits + 1, giving 1..16. Until then: always address 1. */
    return 1u;
}
