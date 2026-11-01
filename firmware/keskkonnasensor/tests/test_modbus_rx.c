/**
 * @file    test_modbus_rx.c
 * @brief   PC unit tests for modbus_rx.c, the frame assembler.
 *
 * Build and run all tests with:   tests/run_tests.sh
 *
 * On the board, the UART interrupt calls modbus_rx_byte(), _error() and
 * _gap(). Here the tests call them directly, in the order the interrupt
 * would, so every situation can be played through without hardware.
 */
#include "modbus_rx.h"
#include "test.h"

static modbus_rx_t rx;
static uint8_t out[MODBUS_RTU_MAX_FRAME];

/* Feed n bytes as if they arrived one after another. */
static void feed(const uint8_t *bytes, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        modbus_rx_byte(&rx, bytes[i]);
    }
}

static const uint8_t frame[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x02, 0x71, 0xCB};
static const uint8_t frame2[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0A};

/* ---- Normal operation -------------------------------------------------- */

static void test_nothing_before_gap(void)
{
    modbus_rx_init(&rx);
    feed(frame, sizeof frame);
    /* All bytes are in, but no silence yet: the frame may not be over. */
    CHECK_EQ_HEX(modbus_rx_take(&rx, out, sizeof out), 0);
}

static void test_frame_after_gap(void)
{
    modbus_rx_init(&rx);
    feed(frame, sizeof frame);
    modbus_rx_gap(&rx);
    size_t n = modbus_rx_take(&rx, out, sizeof out);
    CHECK_BYTES(out, n, frame);
    CHECK_EQ_HEX(rx.dropped, 0);
}

/* A frame can be taken once only. */
static void test_take_once(void)
{
    modbus_rx_init(&rx);
    feed(frame, sizeof frame);
    modbus_rx_gap(&rx);
    modbus_rx_take(&rx, out, sizeof out);
    CHECK_EQ_HEX(modbus_rx_take(&rx, out, sizeof out), 0);
}

static void test_two_frames_in_a_row(void)
{
    modbus_rx_init(&rx);
    feed(frame, sizeof frame);
    modbus_rx_gap(&rx);
    size_t n = modbus_rx_take(&rx, out, sizeof out);
    CHECK_BYTES(out, n, frame);
    feed(frame2, sizeof frame2);
    modbus_rx_gap(&rx);
    n = modbus_rx_take(&rx, out, sizeof out);
    CHECK_BYTES(out, n, frame2);
}

/* A timeout with nothing received (should not happen, but must be safe). */
static void test_gap_without_bytes(void)
{
    modbus_rx_init(&rx);
    modbus_rx_gap(&rx);
    CHECK_EQ_HEX(modbus_rx_take(&rx, out, sizeof out), 0);
    CHECK_EQ_HEX(rx.dropped, 0);
}

/* The largest legal frame, 256 bytes, fits. */
static void test_max_frame(void)
{
    modbus_rx_init(&rx);
    for (unsigned i = 0; i < MODBUS_RTU_MAX_FRAME; i++) {
        modbus_rx_byte(&rx, (uint8_t)i);
    }
    modbus_rx_gap(&rx);
    CHECK_EQ_HEX(modbus_rx_take(&rx, out, sizeof out), MODBUS_RTU_MAX_FRAME);
    CHECK_EQ_HEX(out[255], 0xFF);
}

/* ---- Bad frames are dropped and counted --------------------------------- */

static void test_line_error_drops_frame(void)
{
    modbus_rx_init(&rx);
    feed(frame, 3);
    modbus_rx_error(&rx); /* e.g. parity error on the 4th byte */
    feed(&frame[3], sizeof frame - 3);
    modbus_rx_gap(&rx);
    CHECK_EQ_HEX(modbus_rx_take(&rx, out, sizeof out), 0);
    CHECK_EQ_HEX(rx.dropped, 1);
}

/* After a dropped frame, the next good one comes through normally. */
static void test_recovers_after_error(void)
{
    modbus_rx_init(&rx);
    modbus_rx_error(&rx);
    feed(frame, sizeof frame);
    modbus_rx_gap(&rx);
    feed(frame2, sizeof frame2);
    modbus_rx_gap(&rx);
    size_t n = modbus_rx_take(&rx, out, sizeof out);
    CHECK_BYTES(out, n, frame2);
    CHECK_EQ_HEX(rx.dropped, 1);
}

static void test_too_long_dropped(void)
{
    modbus_rx_init(&rx);
    for (unsigned i = 0; i < MODBUS_RTU_MAX_FRAME + 1u; i++) {
        modbus_rx_byte(&rx, 0x55);
    }
    modbus_rx_gap(&rx);
    CHECK_EQ_HEX(modbus_rx_take(&rx, out, sizeof out), 0);
    CHECK_EQ_HEX(rx.dropped, 1);
}

/* A second frame arrives before the main loop took the first one: the
 * first is kept, the second is dropped and counted. */
static void test_frame_while_busy(void)
{
    modbus_rx_init(&rx);
    feed(frame, sizeof frame);
    modbus_rx_gap(&rx);
    feed(frame2, sizeof frame2); /* main loop has not taken frame yet */
    modbus_rx_gap(&rx);
    size_t n = modbus_rx_take(&rx, out, sizeof out);
    CHECK_BYTES(out, n, frame);
    CHECK_EQ_HEX(rx.dropped, 1);
}

/* The main loop takes the frame while the next one is half received: the
 * rest of that one must not become a "new" frame. */
static void test_take_in_middle_of_next_frame(void)
{
    modbus_rx_init(&rx);
    feed(frame, sizeof frame);
    modbus_rx_gap(&rx);
    feed(frame2, 3);                    /* arrives while frame waits */
    modbus_rx_take(&rx, out, sizeof out);
    feed(&frame2[3], sizeof frame2 - 3); /* the rest of it */
    modbus_rx_gap(&rx);
    CHECK_EQ_HEX(modbus_rx_take(&rx, out, sizeof out), 0);
    CHECK_EQ_HEX(rx.dropped, 1);
    /* ...and the one after that works again. */
    feed(frame, sizeof frame);
    modbus_rx_gap(&rx);
    size_t n = modbus_rx_take(&rx, out, sizeof out);
    CHECK_BYTES(out, n, frame);
}

/* ---- The t3.5 gap -------------------------------------------------------- */

static void test_gap_bits(void)
{
    /* Up to 19200 baud: 3.5 characters, rounded up. */
    CHECK_EQ_HEX(modbus_rx_gap_bits(19200, 11), 39); /* 8E1: 38.5 -> 39 */
    CHECK_EQ_HEX(modbus_rx_gap_bits(9600, 11), 39);
    CHECK_EQ_HEX(modbus_rx_gap_bits(19200, 10), 35); /* 8N1: exactly 35 */
    /* Above 19200: a fixed 1.75 ms. */
    CHECK_EQ_HEX(modbus_rx_gap_bits(38400, 11), 68);   /* 67.2 -> 68   */
    CHECK_EQ_HEX(modbus_rx_gap_bits(115200, 11), 202); /* 201.6 -> 202 */
    CHECK_EQ_HEX(modbus_rx_gap_bits(40000, 11), 70);   /* exactly 70   */
}

int main(void)
{
    /* normal */
    test_nothing_before_gap();
    test_frame_after_gap();
    test_take_once();
    test_two_frames_in_a_row();
    test_gap_without_bytes();
    test_max_frame();
    /* dropped */
    test_line_error_drops_frame();
    test_recovers_after_error();
    test_too_long_dropped();
    test_frame_while_busy();
    test_take_in_middle_of_next_frame();
    /* gap */
    test_gap_bits();
    return test_summary();
}
