/**
 * @file    modbus_rx.c
 * @brief   Collects received bytes into complete Modbus RTU frames.
 *
 * See modbus_rx.h for the events and for who may touch what, when.
 */
#include "modbus_rx.h"

void modbus_rx_init(modbus_rx_t *rx)
{
    rx->len = 0;
    rx->bad = false;
    rx->ready = false;
    rx->lost = false;
    rx->dropped = 0;
}

void modbus_rx_byte(modbus_rx_t *rx, uint8_t byte)
{
    /* The previous frame has not been taken yet (or this frame started
     * while it was waiting): we cannot store this one. Remember to count
     * it as dropped when its gap comes. */
    if (rx->ready || rx->lost) {
        rx->lost = true;
        return;
    }
    if (rx->len < MODBUS_RTU_MAX_FRAME) {
        rx->buf[rx->len] = byte;
        rx->len++;
    } else {
        rx->bad = true; /* longer than any valid frame */
    }
}

void modbus_rx_error(modbus_rx_t *rx)
{
    if (rx->ready || rx->lost) {
        rx->lost = true;
        return;
    }
    rx->bad = true;
}

void modbus_rx_gap(modbus_rx_t *rx)
{
    if (rx->lost) {
        rx->dropped++;
        rx->lost = false;
        return;
    }
    if (rx->ready || rx->len == 0u) {
        rx->bad = false;
        return; /* nothing new arrived */
    }
    if (rx->bad) {
        /* A damaged frame: throw it away and start over. */
        rx->dropped++;
        rx->len = 0;
        rx->bad = false;
        return;
    }
    rx->ready = true; /* hand the buffer to the main loop */
}

size_t modbus_rx_take(modbus_rx_t *rx, uint8_t *out, size_t max)
{
    if (!rx->ready) {
        return 0;
    }
    size_t n = rx->len;
    if (n > max) {
        /* Caller's buffer too small: cannot use the frame. Not counted in
         * `dropped`, because only the interrupt may change that one: an
         * interrupt between our read and our write would lose its count. */
        n = 0;
    }
    for (size_t i = 0; i < n; i++) {
        out[i] = rx->buf[i]; /* a loop, because memcpy() does not take
                                volatile memory */
    }
    rx->len = 0;
    rx->ready = false; /* last: hand the buffer back to the interrupt */
    return n;
}

uint32_t modbus_rx_gap_bits(uint32_t baud, uint32_t bits_per_char)
{
    if (baud > 19200u) {
        /* 1.75 ms = baud * 7 / 4000 bits; "+ 3999" rounds up. */
        return (baud * 7u + 3999u) / 4000u;
    }
    /* 3.5 characters = bits_per_char * 7 / 2; "+ 1" rounds up. */
    return (bits_per_char * 7u + 1u) / 2u;
}
