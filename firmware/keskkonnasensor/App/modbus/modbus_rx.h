/**
 * @file    modbus_rx.h
 * @brief   Collects received bytes into complete Modbus RTU frames.
 *
 * Modbus RTU frames have no length byte and no end marker: a frame ends
 * when the line stays silent for 3.5 character times ("t3.5"). The UART
 * interrupt reports three kinds of events, and this module turns them
 * into frames:
 *
 *     modbus_rx_byte()   a byte arrived
 *     modbus_rx_error()  the byte had a parity, framing or noise error,
 *                        or a byte was lost (overrun)
 *     modbus_rx_gap()    the line has been silent for t3.5: frame over
 *
 * The main loop then takes the finished frame with modbus_rx_take().
 *
 * Two parts of the program use the same data: the interrupt writes it and
 * the main loop reads it, and the interrupt can strike between any two
 * instructions of the main loop. The rule that keeps this safe:
 *   - While no frame is ready, only the interrupt touches buf and len.
 *   - While a frame is ready, the interrupt leaves buf and len alone
 *     (new bytes are thrown away), and only the main loop touches them.
 *   - The `ready` flag hands the buffer from one side to the other.
 * All fields are `volatile`: they change "behind the compiler's back"
 * (in the interrupt), so it must really read them from memory each time
 * instead of keeping an old copy in a register.
 *
 * Portable: no HAL calls, unit-tested on the PC (tests/test_modbus_rx.c).
 */
#ifndef MODBUS_RX_H
#define MODBUS_RX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "modbus_rtu.h" /* MODBUS_RTU_MAX_FRAME */

typedef struct {
    volatile uint8_t buf[MODBUS_RTU_MAX_FRAME];
    volatile uint16_t len;     /* bytes in buf                            */
    volatile bool bad;         /* this frame had an error or overflowed   */
    volatile bool ready;       /* a complete frame waits in buf           */
    volatile bool lost;        /* a frame is arriving while one is ready  */
    volatile uint16_t dropped; /* frames thrown away, only ever counts up */
} modbus_rx_t;

/** @brief  Empty the receiver. */
void modbus_rx_init(modbus_rx_t *rx);

/** @brief  Interrupt: a byte arrived. */
void modbus_rx_byte(modbus_rx_t *rx, uint8_t byte);

/** @brief  Interrupt: a line error; the frame being received is bad. */
void modbus_rx_error(modbus_rx_t *rx);

/** @brief  Interrupt: t3.5 of silence, the frame being received is over. */
void modbus_rx_gap(modbus_rx_t *rx);

/**
 * @brief  Main loop: take the finished frame, if there is one.
 * @param  out  Where to copy it.
 * @param  max  Size of out. A frame longer than this is dropped.
 * @return Frame length in bytes, or 0 if no frame is ready.
 */
size_t modbus_rx_take(modbus_rx_t *rx, uint8_t *out, size_t max);

/**
 * @brief  The t3.5 silence, in bit times, for the UART's receiver timeout.
 *
 * Modbus over Serial Line V1.02, section 2.5.1.1: the frame gap is 3.5
 * character times; above 19200 baud a fixed 1.75 ms is used instead.
 * Both are rounded up to whole bits.
 *
 * @param  baud           Baud rate.
 * @param  bits_per_char  Start + data + parity + stop bits (11 for 8E1).
 */
uint32_t modbus_rx_gap_bits(uint32_t baud, uint32_t bits_per_char);

#endif /* MODBUS_RX_H */
