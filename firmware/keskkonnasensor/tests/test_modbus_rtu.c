/**
 * @file    test_modbus_rtu.c
 * @brief   PC unit tests for modbus_rtu_handle(), the frame parser.
 *
 * Build and run all tests with:   tests/run_tests.sh
 *
 * The tests use a FAKE register provider: two small arrays that stand in
 * for the real register map (step 3). Our board has address 1 and:
 *   - 10 input registers (0..9). Register 9 pretends to be a broken
 *     sensor and always fails.
 *   - 8 holding registers (0..7). Values above 1000 are refused.
 *
 * Where the expected bytes come from:
 *   - The examples from tutorials/frame-parser.html are written out byte
 *     for byte, CRC included. Those CRCs were computed separately with a
 *     shell script, so they also cross-check modbus_crc16().
 *   - The other tests let add_crc() append the CRC. That is fine, because
 *     modbus_crc16() is already proven by test_modbus_crc.c.
 */
#include "modbus_crc.h"
#include "modbus_rtu.h"
#include "test.h"

#define OUR_ADDRESS 1u

/* ---- The fake register provider --------------------------------------- */

#define INPUT_COUNT   10u
#define HOLDING_COUNT 8u
#define BROKEN_INPUT  9u    /* reading this register "fails"       */
#define MAX_SETTING   1000u /* holding values above this are refused */

static uint16_t input_regs[INPUT_COUNT];
static uint16_t holding_regs[HOLDING_COUNT];

static modbus_exception_t fake_read_input(uint16_t start, uint16_t count, uint16_t *values)
{
    if ((uint32_t)start + count > INPUT_COUNT) {
        return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
    }
    for (uint16_t i = 0; i < count; i++) {
        if (start + i == BROKEN_INPUT) {
            return MODBUS_EX_DEVICE_FAILURE;
        }
        values[i] = input_regs[start + i];
    }
    return MODBUS_EX_NONE;
}

static modbus_exception_t fake_read_holding(uint16_t start, uint16_t count, uint16_t *values)
{
    if ((uint32_t)start + count > HOLDING_COUNT) {
        return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
    }
    for (uint16_t i = 0; i < count; i++) {
        values[i] = holding_regs[start + i];
    }
    return MODBUS_EX_NONE;
}

/* Checks everything first, then writes. A refused request changes nothing. */
static modbus_exception_t fake_write_holding(uint16_t start, uint16_t count, const uint16_t *values)
{
    if ((uint32_t)start + count > HOLDING_COUNT) {
        return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
    }
    for (uint16_t i = 0; i < count; i++) {
        if (values[i] > MAX_SETTING) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
    }
    for (uint16_t i = 0; i < count; i++) {
        holding_regs[start + i] = values[i];
    }
    return MODBUS_EX_NONE;
}

static const modbus_regs_t fake_regs = {
    .read_input = fake_read_input,
    .read_holding = fake_read_holding,
    .write_holding = fake_write_holding,
};

/* A second fake where EVERY register 0..0xFFFF exists, and its value is its
 * own number. It never refuses anything, so tests using it prove that the
 * parser's own checks work, not the fake table's. */
static modbus_exception_t every_reg_read(uint16_t start, uint16_t count, uint16_t *values)
{
    for (uint16_t i = 0; i < count; i++) {
        values[i] = (uint16_t)(start + i);
    }
    return MODBUS_EX_NONE;
}

static const modbus_regs_t every_reg = {.read_input = every_reg_read};

/* Put the fake registers back to known values before each test, so one
 * test cannot affect the next. */
static void reset_registers(void)
{
    input_regs[0] = 2500; /* 25.00 degC  */
    input_regs[1] = 5000; /* 50.00 %RH   */
    for (uint16_t i = 2; i < INPUT_COUNT; i++) {
        input_regs[i] = (uint16_t)(0x0100u + i);
    }
    memset(holding_regs, 0, sizeof holding_regs);
}

/* ---- Helpers ----------------------------------------------------------- */

static uint8_t resp[MODBUS_RTU_MAX_FRAME];

/* Run the parser on a frame, with our fake registers and address 1. */
static size_t handle(const uint8_t *req, size_t len)
{
    reset_registers();
    return modbus_rtu_handle(&fake_regs, OUR_ADDRESS, req, len, resp);
}

/* Same, but without resetting the registers first (for write tests that
 * set up a value and then read it back). */
static size_t handle_keep(const uint8_t *req, size_t len)
{
    return modbus_rtu_handle(&fake_regs, OUR_ADDRESS, req, len, resp);
}

/* Write the CRC into the last 2 bytes of a frame. The array must end with
 * two placeholder bytes for it. */
static void add_crc(uint8_t *frame, size_t total_len)
{
    uint16_t crc = modbus_crc16(frame, total_len - 2u);
    frame[total_len - 2u] = (uint8_t)(crc & 0xFFu);
    frame[total_len - 1u] = (uint8_t)(crc >> 8);
}

/* ---- Outcome 1: stay silent ------------------------------------------- */

static void test_empty_frame(void)
{
    const uint8_t req[1] = {0};
    CHECK_EQ_HEX(handle(req, 0), 0);
}

static void test_too_short(void)
{
    const uint8_t req[] = {0x01, 0x04};
    CHECK_EQ_HEX(handle(req, sizeof req), 0);
}

/* Count byte damaged: 02 became 03, so the CRC no longer matches. */
static void test_damaged_frame(void)
{
    const uint8_t req[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x03, 0x71, 0xCB};
    CHECK_EQ_HEX(handle(req, sizeof req), 0);
}

/* A valid frame, but for board 2. */
static void test_other_board(void)
{
    const uint8_t req[] = {0x02, 0x04, 0x00, 0x00, 0x00, 0x02, 0x71, 0xF8};
    CHECK_EQ_HEX(handle(req, sizeof req), 0);
}

/* ---- Outcome 3: normal reply ------------------------------------------ */

/* "Board 1, read input registers 0 and 1." */
static void test_normal_read(void)
{
    const uint8_t req[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x02, 0x71, 0xCB};
    const uint8_t expected[] = {0x01, 0x04, 0x04, 0x09, 0xC4, 0x13, 0x88, 0xB4, 0xB3};
    size_t len = handle(req, sizeof req);
    CHECK_BYTES(resp, len, expected);
}

/* Write 0x0123 (= 291) into holding register 2, then read it back. */
static void test_write_then_read_holding(void)
{
    uint8_t write[] = {0x01, 0x06, 0x00, 0x02, 0x01, 0x23, 0, 0};
    uint8_t read[] = {0x01, 0x03, 0x00, 0x02, 0x00, 0x01, 0, 0};
    uint8_t expected[] = {0x01, 0x03, 0x02, 0x01, 0x23, 0, 0};
    add_crc(write, sizeof write);
    add_crc(read, sizeof read);
    add_crc(expected, sizeof expected);

    handle(write, sizeof write);
    size_t len = handle_keep(read, sizeof read);
    CHECK_BYTES(resp, len, expected);
}

/* Function 06: the reply is an exact copy of the request. */
static void test_write_single(void)
{
    uint8_t req[] = {0x01, 0x06, 0x00, 0x05, 0x00, 0x64, 0, 0};
    add_crc(req, sizeof req);

    size_t len = handle(req, sizeof req);
    CHECK_BYTES(resp, len, req);
    CHECK_EQ_HEX(holding_regs[5], 100);
}

/* Function 16: write 10 and 258 into holding registers 1 and 2. */
static void test_write_multiple(void)
{
    uint8_t req[] = {0x01, 0x10, 0x00, 0x01, 0x00, 0x02, 0x04,
                     0x00, 0x0A, 0x01, 0x02, 0, 0};
    uint8_t expected[] = {0x01, 0x10, 0x00, 0x01, 0x00, 0x02, 0, 0};
    add_crc(req, sizeof req);
    add_crc(expected, sizeof expected);

    size_t len = handle(req, sizeof req);
    CHECK_BYTES(resp, len, expected);
    CHECK_EQ_HEX(holding_regs[1], 10);
    CHECK_EQ_HEX(holding_regs[2], 258);
}

/* ---- Outcome 2: error reply ------------------------------------------- */

/* Function 05 (write coil) is not supported -> error 01. */
static void test_unknown_function(void)
{
    const uint8_t req[] = {0x01, 0x05, 0x00, 0x00, 0xFF, 0x00, 0x8C, 0x3A};
    const uint8_t expected[] = {0x01, 0x85, 0x01, 0x83, 0x50};
    size_t len = handle(req, sizeof req);
    CHECK_BYTES(resp, len, expected);
}

/* "Read 0 registers" is badly filled in -> error 03. */
static void test_read_count_zero(void)
{
    const uint8_t req[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x0A};
    const uint8_t expected[] = {0x01, 0x84, 0x03, 0x03, 0x01};
    size_t len = handle(req, sizeof req);
    CHECK_BYTES(resp, len, expected);
}

/* 126 is one more than the maximum of 125 -> error 03. */
static void test_read_count_too_big(void)
{
    uint8_t req[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x7E, 0, 0};
    uint8_t expected[] = {0x01, 0x84, 0x03, 0, 0};
    add_crc(req, sizeof req);
    add_crc(expected, sizeof expected);
    size_t len = handle(req, sizeof req);
    CHECK_BYTES(resp, len, expected);
}

/* Register 500 does not exist -> error 02. */
static void test_read_register_500(void)
{
    const uint8_t req[] = {0x01, 0x04, 0x01, 0xF4, 0x00, 0x01, 0x71, 0xC4};
    const uint8_t expected[] = {0x01, 0x84, 0x02, 0xC2, 0xC1};
    size_t len = handle(req, sizeof req);
    CHECK_BYTES(resp, len, expected);
}

/* Registers 8, 9, 10: the last one is past the end -> error 02. */
static void test_read_past_end(void)
{
    uint8_t req[] = {0x01, 0x04, 0x00, 0x08, 0x00, 0x03, 0, 0};
    uint8_t expected[] = {0x01, 0x84, 0x02, 0, 0};
    add_crc(req, sizeof req);
    add_crc(expected, sizeof expected);
    size_t len = handle(req, sizeof req);
    CHECK_BYTES(resp, len, expected);
}

/* Start 0xFFFF, count 2 would run past the last possible register.
 * In 16-bit maths 0xFFFF + 2 wraps to 1, which is why the parser
 * calculates in 32 bits. -> error 02.
 * Uses every_reg: the normal fake would refuse this by itself and hide a
 * bug in the parser's check. */
static void test_read_wraps_around(void)
{
    uint8_t req[] = {0x01, 0x04, 0xFF, 0xFF, 0x00, 0x02, 0, 0};
    uint8_t expected[] = {0x01, 0x84, 0x02, 0, 0};
    add_crc(req, sizeof req);
    add_crc(expected, sizeof expected);
    size_t len = modbus_rtu_handle(&every_reg, OUR_ADDRESS, req, sizeof req, resp);
    CHECK_BYTES(resp, len, expected);
}

/* The largest allowed read: 125 registers. The reply is 255 bytes:
 * address, function, byte count 250, 250 data bytes, 2 CRC bytes. */
static void test_read_max_count(void)
{
    uint8_t req[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x7D, 0, 0};
    add_crc(req, sizeof req);
    size_t len = modbus_rtu_handle(&every_reg, OUR_ADDRESS, req, sizeof req, resp);
    CHECK_EQ_HEX(len, 255);
    CHECK_EQ_HEX(resp[2], 250);                      /* byte count       */
    CHECK_EQ_HEX(resp[3 + 2 * 124 + 1], 124);        /* last value, low  */
    CHECK_EQ_HEX(modbus_crc16(resp, len), 0);        /* reply CRC intact */
}

/* One extra byte in a read request: wrong length -> error 03. */
static void test_read_wrong_length(void)
{
    uint8_t req[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x02, 0x00, 0, 0};
    uint8_t expected[] = {0x01, 0x84, 0x03, 0, 0};
    add_crc(req, sizeof req);
    add_crc(expected, sizeof expected);
    size_t len = handle(req, sizeof req);
    CHECK_BYTES(resp, len, expected);
}

/* Register 9 is the "broken sensor" -> error 04. */
static void test_device_failure(void)
{
    uint8_t req[] = {0x01, 0x04, 0x00, 0x09, 0x00, 0x01, 0, 0};
    uint8_t expected[] = {0x01, 0x84, 0x04, 0, 0};
    add_crc(req, sizeof req);
    add_crc(expected, sizeof expected);
    size_t len = handle(req, sizeof req);
    CHECK_BYTES(resp, len, expected);
}

/* 2000 is above the allowed 1000 -> error 03, register unchanged. */
static void test_write_single_value_refused(void)
{
    uint8_t req[] = {0x01, 0x06, 0x00, 0x05, 0x07, 0xD0, 0, 0};
    uint8_t expected[] = {0x01, 0x86, 0x03, 0, 0};
    add_crc(req, sizeof req);
    add_crc(expected, sizeof expected);
    size_t len = handle(req, sizeof req);
    CHECK_BYTES(resp, len, expected);
    CHECK_EQ_HEX(holding_regs[5], 0);
}

/* Says 2 registers but byte count 3 instead of 4 -> error 03. */
static void test_write_multiple_byte_count_wrong(void)
{
    uint8_t req[] = {0x01, 0x10, 0x00, 0x01, 0x00, 0x02, 0x03,
                     0x00, 0x0A, 0x01, 0, 0};
    uint8_t expected[] = {0x01, 0x90, 0x03, 0, 0};
    add_crc(req, sizeof req);
    add_crc(expected, sizeof expected);
    size_t len = handle(req, sizeof req);
    CHECK_BYTES(resp, len, expected);
}

/* First value fine (5), second refused (2000). Nothing may be written,
 * not even the first one. -> error 03. */
static void test_write_multiple_all_or_nothing(void)
{
    uint8_t req[] = {0x01, 0x10, 0x00, 0x01, 0x00, 0x02, 0x04,
                     0x00, 0x05, 0x07, 0xD0, 0, 0};
    uint8_t expected[] = {0x01, 0x90, 0x03, 0, 0};
    add_crc(req, sizeof req);
    add_crc(expected, sizeof expected);
    size_t len = handle(req, sizeof req);
    CHECK_BYTES(resp, len, expected);
    CHECK_EQ_HEX(holding_regs[1], 0);
    CHECK_EQ_HEX(holding_regs[2], 0);
}

/* A device with no holding registers at all answers 03 with error 01. */
static void test_no_holding_registers(void)
{
    const modbus_regs_t input_only = {.read_input = fake_read_input};
    uint8_t req[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0, 0};
    uint8_t expected[] = {0x01, 0x83, 0x01, 0, 0};
    add_crc(req, sizeof req);
    add_crc(expected, sizeof expected);
    size_t len = modbus_rtu_handle(&input_only, OUR_ADDRESS, req, sizeof req, resp);
    CHECK_BYTES(resp, len, expected);
}

/* ---- Broadcast: do it, never reply ------------------------------------ */

/* "Every board: write 100 into holding register 5." */
static void test_broadcast_write(void)
{
    const uint8_t req[] = {0x00, 0x06, 0x00, 0x05, 0x00, 0x64, 0x99, 0xF1};
    CHECK_EQ_HEX(handle(req, sizeof req), 0);
    CHECK_EQ_HEX(holding_regs[5], 100);
}

/* Even an error stays silent on a broadcast. */
static void test_broadcast_error_is_silent(void)
{
    uint8_t req[] = {0x00, 0x05, 0x00, 0x00, 0xFF, 0x00, 0, 0};
    add_crc(req, sizeof req);
    CHECK_EQ_HEX(handle(req, sizeof req), 0);
}

int main(void)
{
    /* stay silent */
    test_empty_frame();
    test_too_short();
    test_damaged_frame();
    test_other_board();
    /* normal reply */
    test_normal_read();
    test_write_then_read_holding();
    test_write_single();
    test_write_multiple();
    test_read_max_count();
    /* error reply */
    test_unknown_function();
    test_read_count_zero();
    test_read_count_too_big();
    test_read_register_500();
    test_read_past_end();
    test_read_wraps_around();
    test_read_wrong_length();
    test_device_failure();
    test_write_single_value_refused();
    test_write_multiple_byte_count_wrong();
    test_write_multiple_all_or_nothing();
    test_no_holding_registers();
    /* broadcast */
    test_broadcast_write();
    test_broadcast_error_is_silent();
    return test_summary();
}
