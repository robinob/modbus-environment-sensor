/**
 * @file    modbus_crc.c
 * @brief   CRC-16/MODBUS, computed bit by bit.
 *
 * How it works
 * ------------
 * A CRC is the remainder of a division. The message is treated as one long
 * binary number and divided by the polynomial. The division uses XOR
 * instead of subtraction (no borrows, so every bit is independent), which
 * makes it possible to process the message one bit at a time with only
 * shifts and XORs.
 *
 * Modbus processes each byte starting from its least significant bit.
 * Because of that the whole algorithm runs "mirrored": the register
 * shifts RIGHT, and the polynomial 0x8005 is used bit-reversed as 0xA001.
 *
 * Why bit by bit and not a lookup table?
 * --------------------------------------
 * A table version precomputes the result for all 256 byte values and is
 * about 8 times faster, but the table takes 512 bytes of flash. Our frames
 * are at most 256 bytes and the CPU runs at 64 MHz, so the slow version
 * still finishes in well under a millisecond. Small code wins here.
 */
#include "modbus_crc.h"

/* Bit-reversed form of the Modbus polynomial 0x8005. */
#define MODBUS_CRC_POLY_REFLECTED 0xA001u

/* Starting value of the CRC register, fixed by the Modbus spec. */
#define MODBUS_CRC_INIT 0xFFFFu

uint16_t modbus_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = MODBUS_CRC_INIT;

    for (size_t i = 0; i < len; i++) {
        /* Mix the next byte into the low 8 bits of the register. */
        crc ^= data[i];

        /* Process the 8 bits of that byte, lowest bit first. */
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x0001u) {
                /* Lowest bit is 1: the polynomial "fits" here, so shift
                 * it out and subtract (XOR) the polynomial.
                 * C promotes uint16_t to int before doing arithmetic, so
                 * the (uint16_t) cast says that keeping only 16 bits of
                 * the result is intended. */
                crc = (uint16_t)((crc >> 1) ^ MODBUS_CRC_POLY_REFLECTED);
            } else {
                /* Lowest bit is 0: nothing to subtract, only shift. */
                crc >>= 1;
            }
        }
    }

    return crc;
}
