/**
 * @file    mb_uart.c
 * @brief   Modbus RTU over USART1: interrupt-driven receive and send.
 *
 * CubeMX (MX_USART1_UART_Init) sets the baud rate, parity and pins. This
 * file then takes over USART1 and drives it through its registers, not
 * through the HAL UART functions. The reason is the receiver timeout: the
 * USART can measure the t3.5 silence itself, which is exactly the Modbus
 * end-of-frame rule. The HAL has no simple receive call that uses it.
 *
 * From RM0444 Rev 6 (checked):
 *   - 33.5.11: the USART supports Modbus/RTU end-of-block detection
 *     through the receiver timeout. (ST's example uses 2 character
 *     times; we use the 3.5 the Modbus spec asks for.)
 *   - 33.5.16: with 1 stop bit, the timeout counts from the end of the
 *     stop bit of the last received character.
 *   - 33.8.7: RTOR may be written while the USART runs.
 *   - ISR register, RTOF note: "The counter counts even if RE = 0 ... If
 *     the timeout has already elapsed when RE is set, then RTOF is set."
 *     So switching the receiver back on after a reply usually gives one
 *     extra timeout with no bytes. modbus_rx_gap() ignores an empty gap.
 *
 * The interrupt handler USART1_IRQHandler() is defined here. The startup
 * file declares that name as "weak": a placeholder that any real function
 * with the same name replaces. So in CubeMX, the USART1 interrupt must
 * stay UNTICKED in NVIC settings; otherwise CubeMX writes its own
 * USART1_IRQHandler into stm32g0xx_it.c and the linker reports the name
 * twice.
 *
 * Sending: RS-485 is one pair of wires shared by everyone, so we talk or
 * listen, never both. The MAX13487E switches its driver on by itself
 * ("AutoDirection"). While we send, the STM32's receiver is switched off
 * too, so our own reply can never come back in as a "request".
 */
#include "mb_uart.h"

#include <stdbool.h>

#include "main.h"     /* the HAL and the register definitions */
#include "modbus_rx.h"
#include "usart.h"     /* huart1: the settings CubeMX made     */

static modbus_rx_t rx;

/* The reply being sent. Points at the caller's buffer, which must stay
 * unchanged until sending is done. That is the case in app.c: a new reply
 * is only built for a new request, and none can arrive while we send,
 * because the receiver is off. */
static const uint8_t *tx_data;
static volatile size_t tx_len;
static volatile size_t tx_pos;
static volatile bool tx_busy;

/* Bits per character from CubeMX's settings: start + data + stop. With
 * parity, CubeMX's "word length" already includes the parity bit (8E1 is
 * a 9-bit word), so it does not need adding separately. */
static uint32_t bits_per_char(void)
{
    uint32_t word = 8u;
    if (huart1.Init.WordLength == UART_WORDLENGTH_9B) {
        word = 9u;
    } else if (huart1.Init.WordLength == UART_WORDLENGTH_7B) {
        word = 7u;
    }
    uint32_t stop = (huart1.Init.StopBits == UART_STOPBITS_2) ? 2u : 1u;
    return 1u + word + stop;
}

void mb_uart_init(void)
{
    USART_TypeDef *u = USART1;

    modbus_rx_init(&rx);
    tx_busy = false;

    /* Configure with the USART switched off (UE = 0): some of these
     * settings may only be changed while it is off. */
    u->CR1 &= ~USART_CR1_UE;

    /* Receiver timeout = the t3.5 frame gap, counted in bit times. */
    u->RTOR = modbus_rx_gap_bits(huart1.Init.BaudRate, bits_per_char());
    u->CR2 |= USART_CR2_RTOEN;

    /* Clear any old flags, then enable two interrupts: "byte received"
     * and "receiver timeout". Parity/framing/noise/overrun errors are
     * flagged together with the received byte, so the byte interrupt
     * sees them too. */
    u->ICR = USART_ICR_PECF | USART_ICR_FECF | USART_ICR_NECF |
             USART_ICR_ORECF | USART_ICR_RTOCF | USART_ICR_TCCF;
    u->CR1 |= USART_CR1_RXNEIE_RXFNEIE | USART_CR1_RTOIE;

    u->CR1 |= USART_CR1_UE;

    /* Let the interrupt through to the CPU (NVIC = the interrupt
     * controller). Priority 0 is the highest: a byte must be read before
     * the next one arrives, about 0.57 ms later at 19200 baud. */
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

size_t mb_uart_receive(uint8_t *frame, size_t max)
{
    return modbus_rx_take(&rx, frame, max);
}

void mb_uart_send(const uint8_t *frame, size_t len)
{
    if (len == 0u || tx_busy) {
        return;
    }
    tx_data = frame;
    tx_len = len;
    tx_pos = 0;
    tx_busy = true;

    USART1->CR1 &= ~USART_CR1_RE;          /* stop listening       */
    USART1->CR1 |= USART_CR1_TXEIE_TXFNFIE; /* "ready for a byte" on */
    /* From here on the interrupt sends the bytes one by one. */
}

uint16_t mb_uart_dropped(void)
{
    return rx.dropped;
}

/* Runs on every USART1 event. Each "if" handles one kind of event; more
 * than one can be pending at the same time. */
void USART1_IRQHandler(void)
{
    USART_TypeDef *u = USART1;
    uint32_t isr = u->ISR; /* snapshot of all event flags */
    uint32_t cr1 = u->CR1;

    /* --- Receiving ------------------------------------------------- */

    /* A line error belongs to the byte now waiting in RDR. Clearing the
     * flag is done by writing 1 to its bit in ICR. */
    if (isr & (USART_ISR_PE | USART_ISR_FE | USART_ISR_NE | USART_ISR_ORE)) {
        u->ICR = USART_ICR_PECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_ORECF;
        modbus_rx_error(&rx);
    }
    /* A byte arrived. Reading RDR clears the flag. With parity on, bit 8
     * of RDR is the parity bit; the cast to uint8_t keeps the data bits. */
    if (isr & USART_ISR_RXNE_RXFNE) {
        modbus_rx_byte(&rx, (uint8_t)u->RDR);
    }
    /* t3.5 of silence after the last byte: the frame is over. */
    if (isr & USART_ISR_RTOF) {
        u->ICR = USART_ICR_RTOCF;
        modbus_rx_gap(&rx);
    }

    /* --- Sending --------------------------------------------------- */

    /* The transmit register is empty: give it the next byte. After the
     * last one, wait for "transmission complete" instead. */
    if ((cr1 & USART_CR1_TXEIE_TXFNFIE) && (isr & USART_ISR_TXE_TXFNF)) {
        if (tx_pos < tx_len) {
            u->TDR = tx_data[tx_pos];
            tx_pos++;
        } else {
            u->CR1 = (u->CR1 & ~USART_CR1_TXEIE_TXFNFIE) | USART_CR1_TCIE;
        }
    }
    /* The last bit has left the wire: listen again. (TXE only means the
     * last byte moved into the shift register; TC means it is fully out.
     * Listening earlier would cut off the end of our own reply.) */
    if ((cr1 & USART_CR1_TCIE) && (isr & USART_ISR_TC)) {
        u->ICR = USART_ICR_TCCF;
        u->CR1 = (u->CR1 & ~USART_CR1_TCIE) | USART_CR1_RE;
        tx_busy = false;
    }
}
