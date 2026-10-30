/**
 * @file    test_modbus_crc.c
 * @brief   PC unit tests for modbus_crc16().
 *
 * This program runs on the PC, not on the STM32. Build and run it from
 * the firmware/keskkonnasensor folder:
 *
 *   gcc -std=c11 -Wall -Wextra -Werror -IApp/modbus -o build/test_modbus_crc.exe tests/test_modbus_crc.c App/modbus/modbus_crc.c
 *   build/test_modbus_crc.exe
 *
 * Where the expected values come from:
 *   - 0x4B37 is the published check value for CRC-16/MODBUS.
 *   - The frame CRCs were computed with an independent shell script
 *     that reproduces 0x4B37, so it implements the same algorithm.
 */
#include <string.h>

#include "modbus_crc.h"
#include "test.h"

/* The standard check: every CRC variant is published with the CRC of the
 * nine ASCII characters "123456789". If this matches, the polynomial,
 * initial value, bit order and final XOR are all correct. */
static void test_check_value(void)
{
    const char *s = "123456789";
    CHECK_EQ_HEX(modbus_crc16((const uint8_t *)s, strlen(s)), 0x4B37);
}

/* With no data the loop never runs, so the result is the initial value. */
static void test_empty_input(void)
{
    CHECK_EQ_HEX(modbus_crc16(NULL, 0), 0xFFFF);
}

/* A real request: slave 1, function 04 (read input registers),
 * start register 0, count 2. On the wire: 01 04 00 00 00 02 71 CB. */
static void test_request_frame(void)
{
    const uint8_t frame[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x02};
    uint16_t crc = modbus_crc16(frame, sizeof frame);

    CHECK_EQ_HEX(crc, 0xCB71);
    /* Byte order on the wire is low byte first: */
    CHECK_EQ_HEX(crc & 0xFF, 0x71); /* sent first  */
    CHECK_EQ_HEX(crc >> 8, 0xCB);   /* sent second */
}

/* The matching reply: 2 registers, values 0x09C4 and 0x1388.
 * On the wire: 01 04 04 09 C4 13 88 B4 B3. */
static void test_reply_frame(void)
{
    const uint8_t frame[] = {0x01, 0x04, 0x04, 0x09, 0xC4, 0x13, 0x88};
    CHECK_EQ_HEX(modbus_crc16(frame, sizeof frame), 0xB3B4);
}

/* CRC over a complete frame, including its own CRC bytes (low byte
 * first), is 0. The receiver will rely on this, so we test it here. */
static void test_full_frame_gives_zero(void)
{
    const uint8_t frame[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x02, 0x71, 0xCB};
    CHECK_EQ_HEX(modbus_crc16(frame, sizeof frame), 0x0000);
}

/* A single flipped bit must be detected. Here one bit of the register
 * count is wrong (0x02 -> 0x03), so the check over the full frame is no
 * longer 0, and the receiver will drop the frame.
 *
 * We check the exact value, not just "not 0". A broken CRC function also
 * returns "not 0", so a weaker check would pass even when the CRC is
 * wrong. Exact values catch both kinds of bug. */
static void test_corrupted_frame_detected(void)
{
    const uint8_t frame[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x03, 0x71, 0xCB};
    CHECK_EQ_HEX(modbus_crc16(frame, sizeof frame), 0xC051);
}

int main(void)
{
    test_check_value();
    test_empty_input();
    test_request_frame();
    test_reply_frame();
    test_full_frame_gives_zero();
    test_corrupted_frame_detected();
    return test_summary();
}
