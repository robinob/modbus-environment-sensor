/**
 * @file    modbus_crc.h
 * @brief   CRC-16 checksum used by Modbus RTU.
 *
 * Every Modbus RTU frame ends with a 2-byte CRC computed over all bytes
 * before it. The receiver computes the CRC again and compares. If they
 * differ, at least one bit was corrupted on the wire and the frame is
 * silently dropped.
 *
 * Parameters of this CRC (the "CRC-16/MODBUS" variant):
 *   - polynomial 0x8005, used in bit-reversed form 0xA001
 *   - initial value 0xFFFF
 *   - no final XOR
 *   - check value: the ASCII string "123456789" gives 0x4B37
 * Source: Modbus over Serial Line Specification V1.02, section 6.2.2,
 * and the CRC catalogue entry for CRC-16/MODBUS (check value 0x4B37).
 *
 * This file has no hardware dependencies, so it builds both for the STM32
 * and for the PC unit tests.
 */
#ifndef MODBUS_CRC_H
#define MODBUS_CRC_H

/* Include guard: the #ifndef / #define / #endif trio makes sure this
 * header's contents are seen only once per .c file, even if it gets
 * included several times through other headers. */

#include <stddef.h> /* size_t: unsigned type for sizes and lengths   */
#include <stdint.h> /* uint8_t, uint16_t: integers of an exact width */

/**
 * @brief  Compute the Modbus CRC-16 over a block of bytes.
 *
 * @param  data  Pointer to the first byte. May be NULL only if len is 0.
 * @param  len   Number of bytes to include.
 * @return The 16-bit CRC.
 *
 * Byte order on the wire: Modbus sends the CRC LOW byte first, which is
 * the opposite of register values (high byte first). So a CRC of 0xCB71
 * is sent as 0x71, then 0xCB.
 *
 * Handy property for receiving: computing the CRC over a whole frame
 * INCLUDING its two CRC bytes gives 0 when the frame is intact. The
 * receiver can use that instead of extracting and comparing the CRC.
 */
uint16_t modbus_crc16(const uint8_t *data, size_t len);

#endif /* MODBUS_CRC_H */
