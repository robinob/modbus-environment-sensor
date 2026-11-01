/**
 * @file    modbus_rtu.h
 * @brief   Modbus RTU slave: checks a received frame and builds the reply.
 *
 * What this module does
 * ---------------------
 * One function, modbus_rtu_handle(), takes a complete received frame and
 * decides one of three outcomes (see tutorials/frame-parser.html):
 *
 *   1. Stay silent   - frame too short, CRC wrong, or not addressed to us.
 *                      Also: every broadcast (address 0), even on errors.
 *   2. Error reply   - the frame is fine, but the request cannot be done.
 *   3. Normal reply  - the request was done; the reply carries the result.
 *
 * What this module does NOT do
 * ----------------------------
 * - It does not touch the UART. Somebody else collects the bytes and sends
 *   the reply. That keeps this file free of hardware, so it is unit-tested
 *   on the PC.
 * - It does not know what the registers mean. It asks a "register
 *   provider" (a set of callback functions, see modbus_regs_t) for values.
 *   In the firmware that is regmap.c; in the tests it is a fake table.
 *
 * Supported function codes:
 *   0x03 Read Holding Registers     0x06 Write Single Register
 *   0x04 Read Input Registers       0x10 Write Multiple Registers
 *
 * Source for all limits and error codes: MODBUS Application Protocol
 * Specification V1.1b3, sections 6.3, 6.4, 6.6, 6.12 and 7.
 * Framing and broadcast rules: Modbus over Serial Line V1.02, 2.1 and 2.5.
 */
#ifndef MODBUS_RTU_H
#define MODBUS_RTU_H

#include <stddef.h>
#include <stdint.h>

/* Largest possible RTU frame: 1 address + 253 PDU bytes + 2 CRC bytes.
 * Both the receive buffer and the reply buffer must be this big. */
#define MODBUS_RTU_MAX_FRAME 256u

/* Address 0 means "every slave on the bus". */
#define MODBUS_BROADCAST_ADDRESS 0u

/**
 * Modbus exception codes, sent back in an error reply.
 * MODBUS_EX_NONE (0) is not sent; it means "no error".
 */
typedef enum {
    MODBUS_EX_NONE                 = 0x00,
    MODBUS_EX_ILLEGAL_FUNCTION     = 0x01, /* function code not supported */
    MODBUS_EX_ILLEGAL_DATA_ADDRESS = 0x02, /* register does not exist     */
    MODBUS_EX_ILLEGAL_DATA_VALUE   = 0x03, /* request badly formed, or a
                                              value we refuse to write    */
    MODBUS_EX_DEVICE_FAILURE       = 0x04  /* we tried, and it failed     */
} modbus_exception_t;

/**
 * A callback that reads "count" registers starting at "start" into
 * values[0..count-1].
 *
 * Return MODBUS_EX_NONE on success, MODBUS_EX_ILLEGAL_DATA_ADDRESS if any
 * of the registers does not exist, or MODBUS_EX_DEVICE_FAILURE if the value
 * cannot be produced (for example the sensor is missing).
 */
typedef modbus_exception_t (*modbus_read_fn)(uint16_t start, uint16_t count,
                                             uint16_t *values);

/**
 * A callback that writes values[0..count-1] into "count" registers starting
 * at "start".
 *
 * It must check ALL registers and values before changing any of them, so a
 * rejected request leaves everything as it was. Return MODBUS_EX_NONE,
 * MODBUS_EX_ILLEGAL_DATA_ADDRESS, or MODBUS_EX_ILLEGAL_DATA_VALUE for a
 * value that is out of range for that register.
 */
typedef modbus_exception_t (*modbus_write_fn)(uint16_t start, uint16_t count,
                                              const uint16_t *values);

/**
 * The register provider: the "warehouse" the parser asks.
 * Set a pointer to NULL if the device has no registers of that kind; the
 * matching function codes then answer "illegal function".
 */
typedef struct {
    modbus_read_fn  read_input;    /* 0x04: measurements, read-only */
    modbus_read_fn  read_holding;  /* 0x03: settings, read          */
    modbus_write_fn write_holding; /* 0x06, 0x10: settings, write   */
} modbus_regs_t;

/**
 * @brief  Process one received frame and build the reply.
 *
 * @param  regs         Register provider (callbacks).
 * @param  own_address  Our slave address, 1..247.
 * @param  req          The received frame, including its 2 CRC bytes.
 * @param  req_len      Number of bytes in req.
 * @param  resp         Buffer for the reply. Must hold MODBUS_RTU_MAX_FRAME
 *                      bytes.
 * @return Number of reply bytes to send, CRC included.
 *         0 means: stay silent, send nothing.
 */
size_t modbus_rtu_handle(const modbus_regs_t *regs, uint8_t own_address,
                         const uint8_t *req, size_t req_len, uint8_t *resp);

#endif /* MODBUS_RTU_H */
