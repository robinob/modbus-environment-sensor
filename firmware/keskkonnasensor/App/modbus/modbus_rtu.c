/**
 * @file    modbus_rtu.c
 * @brief   Modbus RTU slave frame handling. See modbus_rtu.h.
 *
 * Frame layout (all multi-byte numbers are big-endian, high byte first,
 * EXCEPT the CRC, which is low byte first):
 *
 *   request:   [address] [function] [data ...] [CRC lo] [CRC hi]
 *   reply:     [address] [function] [data ...] [CRC lo] [CRC hi]
 *   error:     [address] [function | 0x80] [exception code] [CRC lo] [CRC hi]
 *
 * The checks run in a fixed order, the same as in the flowchart:
 *   length -> CRC -> address -> function -> request format -> registers.
 */
#include "modbus_rtu.h"

#include <stdbool.h>

#include "modbus_crc.h"

/* Function codes we support. */
#define FC_READ_HOLDING_REGISTERS   0x03u
#define FC_READ_INPUT_REGISTERS     0x04u
#define FC_WRITE_SINGLE_REGISTER    0x06u
#define FC_WRITE_MULTIPLE_REGISTERS 0x10u

/* An error reply sets the top bit of the function code: 0x04 -> 0x84. */
#define EXCEPTION_FLAG 0x80u

/* Smallest valid frame: address + function + 2 CRC bytes. */
#define MIN_FRAME_LEN 4u

/* Limits from the Modbus spec. They come from the frame size: 125
 * registers = 250 data bytes, which is what fits in one 256-byte frame. */
#define MAX_READ_COUNT  125u /* 0x7D, functions 0x03 and 0x04 */
#define MAX_WRITE_COUNT 123u /* 0x7B, function 0x10           */

/* Register addresses are 16-bit, so 0x0000..0xFFFF. A request for "start +
 * count" registers must not run past the end of that space. */
#define REGISTER_SPACE 0x10000u

/* ---- Small helpers for big-endian 16-bit numbers ---------------------- */

/* Read a 16-bit number stored as two bytes, high byte first. */
static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

/* Store a 16-bit number as two bytes, high byte first. */
static void put_u16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)(value & 0xFFu);
}

/* ---- One handler per function code -------------------------------------
 *
 * Every handler gets:
 *   data, len  the request bytes after the function code (CRC removed)
 *   out        where to write the reply bytes after the function code
 *   out_len    set to the number of bytes written to out
 * and returns MODBUS_EX_NONE or the exception code to send back.
 */

/* 0x03 and 0x04. Request data: start (2 bytes), count (2 bytes).
 * Reply data: byte count (1 byte), then count register values. */
static modbus_exception_t read_registers(modbus_read_fn read, const uint8_t *data,
                                         size_t len, uint8_t *out, size_t *out_len)
{
    /* No callback means this device has no registers of this kind. */
    if (read == NULL) {
        return MODBUS_EX_ILLEGAL_FUNCTION;
    }
    if (len != 4u) {
        return MODBUS_EX_ILLEGAL_DATA_VALUE; /* wrong request length */
    }

    uint16_t start = get_u16(&data[0]);
    uint16_t count = get_u16(&data[2]);

    if (count < 1u || count > MAX_READ_COUNT) {
        return MODBUS_EX_ILLEGAL_DATA_VALUE;
    }
    /* The cast to uint32_t matters: in 16 bits, 0xFFFF + 2 would wrap
     * around to 1 and look valid. */
    if ((uint32_t)start + count > REGISTER_SPACE) {
        return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
    }

    /* Temporary space for the values, on the stack: 250 bytes, given back
     * when the function returns. Fine for our 1 KB stack. */
    uint16_t values[MAX_READ_COUNT];
    modbus_exception_t ex = read(start, count, values);
    if (ex != MODBUS_EX_NONE) {
        return ex;
    }

    out[0] = (uint8_t)(count * 2u); /* byte count */
    for (uint16_t i = 0; i < count; i++) {
        put_u16(&out[1u + 2u * i], values[i]);
    }
    *out_len = 1u + 2u * (size_t)count;
    return MODBUS_EX_NONE;
}

/* 0x06. Request data: register (2 bytes), value (2 bytes).
 * Reply: an exact copy of the request data ("echo"). */
static modbus_exception_t write_single(modbus_write_fn write, const uint8_t *data,
                                       size_t len, uint8_t *out, size_t *out_len)
{
    if (write == NULL) {
        return MODBUS_EX_ILLEGAL_FUNCTION;
    }
    if (len != 4u) {
        return MODBUS_EX_ILLEGAL_DATA_VALUE;
    }

    uint16_t reg = get_u16(&data[0]);
    uint16_t value = get_u16(&data[2]);

    modbus_exception_t ex = write(reg, 1u, &value);
    if (ex != MODBUS_EX_NONE) {
        return ex;
    }

    for (size_t i = 0; i < 4u; i++) {
        out[i] = data[i];
    }
    *out_len = 4u;
    return MODBUS_EX_NONE;
}

/* 0x10. Request data: start (2), count (2), byte count (1), values (2 each).
 * Reply data: start (2), count (2). */
static modbus_exception_t write_multiple(modbus_write_fn write, const uint8_t *data,
                                         size_t len, uint8_t *out, size_t *out_len)
{
    if (write == NULL) {
        return MODBUS_EX_ILLEGAL_FUNCTION;
    }
    if (len < 5u) {
        return MODBUS_EX_ILLEGAL_DATA_VALUE;
    }

    uint16_t start = get_u16(&data[0]);
    uint16_t count = get_u16(&data[2]);
    uint8_t byte_count = data[4];

    /* Three ways the request can contradict itself. All are "badly filled
     * in", so all give ILLEGAL_DATA_VALUE. */
    if (count < 1u || count > MAX_WRITE_COUNT) {
        return MODBUS_EX_ILLEGAL_DATA_VALUE;
    }
    if (byte_count != count * 2u) {
        return MODBUS_EX_ILLEGAL_DATA_VALUE;
    }
    if (len != 5u + byte_count) {
        return MODBUS_EX_ILLEGAL_DATA_VALUE;
    }
    if ((uint32_t)start + count > REGISTER_SPACE) {
        return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
    }

    uint16_t values[MAX_WRITE_COUNT];
    for (uint16_t i = 0; i < count; i++) {
        values[i] = get_u16(&data[5u + 2u * i]);
    }

    modbus_exception_t ex = write(start, count, values);
    if (ex != MODBUS_EX_NONE) {
        return ex;
    }

    put_u16(&out[0], start);
    put_u16(&out[2], count);
    *out_len = 4u;
    return MODBUS_EX_NONE;
}

/* ---- The public function ---------------------------------------------- */

size_t modbus_rtu_handle(const modbus_regs_t *regs, uint8_t own_address,
                         const uint8_t *req, size_t req_len, uint8_t *resp)
{
    /* Check 1: length. Anything shorter than 4 bytes cannot be a frame;
     * anything longer than 256 is not one either. */
    if (req_len < MIN_FRAME_LEN || req_len > MODBUS_RTU_MAX_FRAME) {
        return 0;
    }

    /* Check 2: CRC. Computing the CRC over the whole frame, its own two
     * CRC bytes included, gives 0 for an intact frame (tested in
     * test_modbus_crc.c). A damaged frame is dropped without a reply: we
     * cannot even trust its address byte. */
    if (modbus_crc16(req, req_len) != 0u) {
        return 0;
    }

    /* Check 3: address. Only answer frames addressed to us. Broadcasts
     * are processed, but get no reply (see the end of this function). */
    uint8_t address = req[0];
    bool broadcast = (address == MODBUS_BROADCAST_ADDRESS);
    if (!broadcast && address != own_address) {
        return 0;
    }

    /* The frame is valid and for us. Split it into its parts. */
    uint8_t function = req[1];
    const uint8_t *data = &req[2];
    size_t data_len = req_len - MIN_FRAME_LEN; /* minus address, function, CRC */

    /* The reply data goes after the address and function bytes. */
    uint8_t *out = &resp[2];
    size_t out_len = 0;
    modbus_exception_t ex;

    /* Check 4 onward: which function, then the handler does the rest. */
    switch (function) {
    case FC_READ_HOLDING_REGISTERS:
        ex = read_registers(regs->read_holding, data, data_len, out, &out_len);
        break;
    case FC_READ_INPUT_REGISTERS:
        ex = read_registers(regs->read_input, data, data_len, out, &out_len);
        break;
    case FC_WRITE_SINGLE_REGISTER:
        ex = write_single(regs->write_holding, data, data_len, out, &out_len);
        break;
    case FC_WRITE_MULTIPLE_REGISTERS:
        ex = write_multiple(regs->write_holding, data, data_len, out, &out_len);
        break;
    default:
        ex = MODBUS_EX_ILLEGAL_FUNCTION;
        break;
    }

    /* A broadcast never gets a reply, not even an error reply. If every
     * board answered at once, the replies would collide on the bus. */
    if (broadcast) {
        return 0;
    }

    /* Build the reply: address, function, data. */
    resp[0] = own_address;
    if (ex == MODBUS_EX_NONE) {
        resp[1] = function;
    } else {
        resp[1] = (uint8_t)(function | EXCEPTION_FLAG);
        resp[2] = (uint8_t)ex;
        out_len = 1u;
    }
    size_t len = 2u + out_len;

    /* Append the CRC, low byte first. */
    uint16_t crc = modbus_crc16(resp, len);
    resp[len] = (uint8_t)(crc & 0xFFu);
    resp[len + 1u] = (uint8_t)(crc >> 8);
    return len + 2u;
}
