/**
 * @file    mb_uart.c
 * @brief   Modbus UART driver. STUB: filled in by step 4.
 *
 * Until then nothing is ever received, so app_poll() never calls the
 * parser and the board stays silent on the bus.
 */
#include "mb_uart.h"

void mb_uart_init(void)
{
    /* Step 4: start receiving on USART1 and set up the 3.5-character
     * silence detection (the receiver timeout, see RM0444). */
}

size_t mb_uart_receive(uint8_t *frame, size_t max)
{
    /* (void) tells the compiler "unused on purpose", so -Wextra does not
     * warn about these parameters while this is a stub. */
    (void)frame;
    (void)max;
    return 0u;
}

void mb_uart_send(const uint8_t *frame, size_t len)
{
    (void)frame;
    (void)len;
}
