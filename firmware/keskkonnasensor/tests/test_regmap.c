/**
 * @file    test_regmap.c
 * @brief   PC unit tests for the register map (regmap.c).
 *
 * Build and run all tests with:   tests/run_tests.sh
 *
 * Most tests call the three callbacks in `regmap` directly, the same way
 * the parser does. The last two send whole Modbus frames through
 * modbus_rtu_handle(), to check that the parser and the real register map
 * fit together.
 */
#include "modbus_crc.h"
#include "modbus_rtu.h"
#include "regmap.h"
#include "test.h"
#include "version.h"

/* ---- Helpers ----------------------------------------------------------- */

static uint16_t vals[125];

/* Read one input register, or return 0xDEAD if the read was refused. */
static uint16_t input(uint16_t addr)
{
    return regmap.read_input(addr, 1, vals) == MODBUS_EX_NONE ? vals[0] : 0xDEADu;
}

/* Read one holding register, or 0xDEAD if refused. */
static uint16_t holding(uint16_t addr)
{
    return regmap.read_holding(addr, 1, vals) == MODBUS_EX_NONE ? vals[0] : 0xDEADu;
}

/* Write one holding register; returns the exception code. */
static modbus_exception_t write1(uint16_t addr, uint16_t value)
{
    return regmap.write_holding(addr, 1, &value);
}

/* ---- After start-up ---------------------------------------------------- */

static void test_default_settings(void)
{
    regmap_init();
    const uint16_t expected[REGMAP_HOLDING_COUNT] = {10, 0, 0, 2, 50, 1000, 1500, 1};
    CHECK_EQ_HEX(regmap.read_holding(0, REGMAP_HOLDING_COUNT, vals), MODBUS_EX_NONE);
    for (uint16_t i = 0; i < REGMAP_HOLDING_COUNT; i++) {
        CHECK_EQ_HEX(vals[i], expected[i]);
    }
}

/* Before the first sensor reading, every measurement says "invalid". */
static void test_measurements_invalid_at_start(void)
{
    regmap_init();
    CHECK_EQ_HEX(input(0), 0x8000); /* temperature, signed  */
    CHECK_EQ_HEX(input(1), 0xFFFF); /* humidity             */
    CHECK_EQ_HEX(input(2), 0x8000); /* dew point, signed    */
    CHECK_EQ_HEX(input(3), 0xFFFF); /* absolute humidity    */
    CHECK_EQ_HEX(input(4), 0xFFFF); /* AQI-UBA              */
    CHECK_EQ_HEX(input(5), 0xFFFF); /* TVOC                 */
    CHECK_EQ_HEX(input(6), 0xFFFF); /* eCO2                 */
    CHECK_EQ_HEX(input(7), 0xFFFF); /* ENS160 validity      */
    CHECK_EQ_HEX(input(11), 0xFFFF); /* SHT45 age: never read */
    CHECK_EQ_HEX(input(12), 0xFFFF); /* ENS160 age           */
    for (uint16_t a = 30; a < 34u; a++) {
        CHECK_EQ_HEX(input(a), 0xFFFF); /* raw values        */
    }
    CHECK_EQ_HEX(input(10), 0);     /* status: nothing ok   */
    CHECK_EQ_HEX(input(15), 0);     /* counters start at 0  */
}

/* ---- Reading measurements --------------------------------------------- */

static void test_read_values(void)
{
    regmap_init();
    regmap_in.temperature = 2345; /* 23.45 degC */
    regmap_in.humidity = 4567;    /* 45.67 %RH  */
    regmap_in.eco2_ppm = 650;
    CHECK_EQ_HEX(input(0), 2345);
    CHECK_EQ_HEX(input(1), 4567);
    CHECK_EQ_HEX(input(6), 650);
}

/* Every field gets a different value, then every address is checked.
 * This catches a register wired to the wrong field. */
static void test_every_address(void)
{
    regmap_init();
    regmap_in.temperature = 0x0100;
    regmap_in.humidity = 0x0101;
    regmap_in.dew_point = 0x0102;
    regmap_in.abs_humidity = 0x0103;
    regmap_in.aqi_uba = 0x0104;
    regmap_in.tvoc_ppb = 0x0105;
    regmap_in.eco2_ppm = 0x0106;
    regmap_in.ens160_validity = 0x0107;
    regmap_in.status = 0x0110;
    regmap_in.sht45_age_s = 0x0111;
    regmap_in.ens160_age_s = 0x0112;
    regmap_in.uptime_s = 0x01130114u;
    regmap_in.cnt_requests = 0x0115;
    regmap_in.cnt_crc_errors = 0x0116;
    regmap_in.cnt_sht45_err = 0x0117;
    regmap_in.cnt_ens160_err = 0x0118;
    regmap_in.slave_address = 0x0121;
    regmap_in.sht45_serial = 0x01220123u;
    regmap_in.ens160_part_id = 0x0124;
    regmap_in.ens160_fw = 0x0125;
    regmap_in.ens160_fw_rel = 0x0126;
    for (uint16_t i = 0; i < 4u; i++) {
        regmap_in.ens160_raw[i] = (uint16_t)(0x0130u + i);
    }

    /* Each value is 0x01 followed by its address's decimal digits
     * (address 21 -> 0x0121), so a wrong one is easy to spot. Reserved
     * addresses read 0, and address 20 is the firmware version. */
    const uint16_t expected[REGMAP_INPUT_COUNT] = {
        0x0100, 0x0101, 0x0102, 0x0103, 0x0104, 0x0105, 0x0106, 0x0107, 0, 0,
        0x0110, 0x0111, 0x0112, 0x0113, 0x0114, 0x0115, 0x0116, 0x0117, 0x0118, 0,
        (FW_VERSION_MAJOR << 8) | FW_VERSION_MINOR,
        0x0121, 0x0122, 0x0123, 0x0124, 0x0125, 0x0126, 0, 0, 0,
        0x0130, 0x0131, 0x0132, 0x0133,
    };
    CHECK_EQ_HEX(regmap.read_input(0, REGMAP_INPUT_COUNT, vals), MODBUS_EX_NONE);
    for (uint16_t a = 0; a < REGMAP_INPUT_COUNT; a++) {
        if (vals[a] != expected[a]) {
            printf("    input register %u:\n", (unsigned)a);
        }
        CHECK_EQ_HEX(vals[a], expected[a]);
    }
}

/* -5.20 degC is -520, sent in two's complement: 0x10000 - 520 = 0xFDF8. */
static void test_negative_temperature(void)
{
    regmap_init();
    regmap_in.temperature = -520;
    CHECK_EQ_HEX(input(0), 0xFDF8);
}

/* 32-bit values: high half in the first register. */
static void test_32bit_word_order(void)
{
    regmap_init();
    regmap_in.uptime_s = 0x00012345u;
    regmap_in.sht45_serial = 0xA1B2C3D4u;
    CHECK_EQ_HEX(input(13), 0x0001);
    CHECK_EQ_HEX(input(14), 0x2345);
    CHECK_EQ_HEX(input(22), 0xA1B2);
    CHECK_EQ_HEX(input(23), 0xC3D4);
}

static void test_firmware_version(void)
{
    regmap_init();
    CHECK_EQ_HEX(input(20), (FW_VERSION_MAJOR << 8) | FW_VERSION_MINOR);
}

/* Reserved registers inside the map read as 0, even if the struct next
 * to them holds something. */
static void test_reserved_read_zero(void)
{
    regmap_init();
    const uint16_t reserved[] = {8, 9, 19, 27, 28, 29};
    for (size_t i = 0; i < sizeof reserved / sizeof reserved[0]; i++) {
        CHECK_EQ_HEX(input(reserved[i]), 0);
    }
}

/* The whole block 0..33 in one request works; one register more does not. */
static void test_input_block_edges(void)
{
    regmap_init();
    CHECK_EQ_HEX(regmap.read_input(0, REGMAP_INPUT_COUNT, vals), MODBUS_EX_NONE);
    CHECK_EQ_HEX(regmap.read_input(30, 5, vals), MODBUS_EX_ILLEGAL_DATA_ADDRESS);
    CHECK_EQ_HEX(regmap.read_input(34, 1, vals), MODBUS_EX_ILLEGAL_DATA_ADDRESS);
}

/* ---- Writing settings -------------------------------------------------- */

static void test_write_setting(void)
{
    regmap_init();
    CHECK_EQ_HEX(write1(0, 60), MODBUS_EX_NONE);
    CHECK_EQ_HEX(holding(0), 60);
    CHECK_EQ_HEX(regmap_set.interval_s, 60);
}

/* -5.00 degC offset = -500 = 0xFE0C on the wire. */
static void test_write_negative_offset(void)
{
    regmap_init();
    CHECK_EQ_HEX(write1(1, 0xFE0C), MODBUS_EX_NONE);
    CHECK_EQ_HEX(holding(1), 0xFE0C);
    CHECK_EQ_HEX(regmap_set.temp_offset == -500, 1);
}

/* Every limit, just outside: refused with error 03, and nothing changes.
 * The edges themselves must be accepted. */
static void test_write_limits(void)
{
    regmap_init();
    struct { uint16_t addr, bad_low, ok_low, ok_high, bad_high; } t[] = {
        {0, 0,      1,      3600,   3601},
        {1, 0xFC17, 0xFC18, 1000,   1001},  /* -1001, -1000 */
        {2, 0xFC17, 0xFC18, 1000,   1001},
        {3, 0xFFFF, 0,      2,      3},     /* no negative modes */
        {4, 0xFFFF, 0,      100,    101},
        {5, 399,    400,    65000,  65001},
        {6, 399,    400,    65000,  65001},
        {7, 0xFFFF, 0,      1,      2},
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        uint16_t before = holding(t[i].addr);
        CHECK_EQ_HEX(write1(t[i].addr, t[i].bad_low), MODBUS_EX_ILLEGAL_DATA_VALUE);
        CHECK_EQ_HEX(write1(t[i].addr, t[i].bad_high), MODBUS_EX_ILLEGAL_DATA_VALUE);
        CHECK_EQ_HEX(holding(t[i].addr), before);
        CHECK_EQ_HEX(write1(t[i].addr, t[i].ok_low), MODBUS_EX_NONE);
        CHECK_EQ_HEX(write1(t[i].addr, t[i].ok_high), MODBUS_EX_NONE);
    }
}

/* One bad value in a multi-register write: nothing is written. */
static void test_write_all_or_nothing(void)
{
    regmap_init();
    const uint16_t v[3] = {60, 100, 5000}; /* 5000 is a bad hum_offset */
    CHECK_EQ_HEX(regmap.write_holding(0, 3, v), MODBUS_EX_ILLEGAL_DATA_VALUE);
    CHECK_EQ_HEX(holding(0), 10); /* interval still the default */
    CHECK_EQ_HEX(holding(1), 0);
}

static void test_holding_edges(void)
{
    regmap_init();
    uint16_t v[2] = {1, 1};
    CHECK_EQ_HEX(regmap.write_holding(7, 2, v), MODBUS_EX_ILLEGAL_DATA_ADDRESS);
    CHECK_EQ_HEX(write1(8, 1), MODBUS_EX_ILLEGAL_DATA_ADDRESS);
    CHECK_EQ_HEX(regmap.read_holding(8, 1, vals), MODBUS_EX_ILLEGAL_DATA_ADDRESS);
    CHECK_EQ_HEX(regmap.read_holding(99, 2, vals), MODBUS_EX_ILLEGAL_DATA_ADDRESS);
}

/* ---- The command register ---------------------------------------------- */

static void test_command(void)
{
    regmap_init();
    CHECK_EQ_HEX(regmap_take_command(), REGMAP_CMD_NONE);
    CHECK_EQ_HEX(write1(REGMAP_CMD_ADDRESS, REGMAP_CMD_SHT45_HEATER), MODBUS_EX_NONE);
    CHECK_EQ_HEX(regmap_take_command(), REGMAP_CMD_SHT45_HEATER);
    CHECK_EQ_HEX(regmap_take_command(), REGMAP_CMD_NONE); /* taken once */
    CHECK_EQ_HEX(holding(REGMAP_CMD_ADDRESS), 0);         /* reads as 0 */
}

static void test_command_invalid(void)
{
    regmap_init();
    CHECK_EQ_HEX(write1(REGMAP_CMD_ADDRESS, 0), MODBUS_EX_ILLEGAL_DATA_VALUE);
    CHECK_EQ_HEX(write1(REGMAP_CMD_ADDRESS, REGMAP_CMD_LAST + 1u), MODBUS_EX_ILLEGAL_DATA_VALUE);
    uint16_t v[2] = {1, 1};
    CHECK_EQ_HEX(regmap.write_holding(REGMAP_CMD_ADDRESS, 2, v), MODBUS_EX_ILLEGAL_DATA_ADDRESS);
    CHECK_EQ_HEX(regmap_take_command(), REGMAP_CMD_NONE);
}

/* A newer command replaces one that was not taken yet. */
static void test_command_replaced(void)
{
    regmap_init();
    write1(REGMAP_CMD_ADDRESS, REGMAP_CMD_SHT45_HEATER);
    write1(REGMAP_CMD_ADDRESS, REGMAP_CMD_RESET_COUNTERS);
    CHECK_EQ_HEX(regmap_take_command(), REGMAP_CMD_RESET_COUNTERS);
}

/* regmap_init() undoes every change. */
static void test_init_resets(void)
{
    regmap_init();
    write1(0, 60);
    write1(REGMAP_CMD_ADDRESS, REGMAP_CMD_SHT45_HEATER);
    regmap_in.cnt_requests = 7;
    regmap_init();
    CHECK_EQ_HEX(holding(0), 10);
    CHECK_EQ_HEX(input(15), 0);
    CHECK_EQ_HEX(regmap_take_command(), REGMAP_CMD_NONE);
}

/* ---- Through the parser ------------------------------------------------ */

static uint8_t resp[MODBUS_RTU_MAX_FRAME];

static void add_crc(uint8_t *frame, size_t total_len)
{
    uint16_t crc = modbus_crc16(frame, total_len - 2u);
    frame[total_len - 2u] = (uint8_t)(crc & 0xFFu);
    frame[total_len - 1u] = (uint8_t)(crc >> 8);
}

/* A master reads temperature and humidity: 23.45 degC = 0x0929,
 * 45.67 %RH = 0x11D7. */
static void test_frame_read_temperature(void)
{
    regmap_init();
    regmap_in.temperature = 2345;
    regmap_in.humidity = 4567;
    const uint8_t req[] = {0x01, 0x04, 0x00, 0x00, 0x00, 0x02, 0x71, 0xCB};
    uint8_t expected[] = {0x01, 0x04, 0x04, 0x09, 0x29, 0x11, 0xD7, 0, 0};
    add_crc(expected, sizeof expected);
    size_t len = modbus_rtu_handle(&regmap, 1, req, sizeof req, resp);
    CHECK_BYTES(resp, len, expected);
}

/* A master sets LED mode 7, which does not exist: error 03. */
static void test_frame_write_refused(void)
{
    regmap_init();
    uint8_t req[] = {0x01, 0x06, 0x00, 0x03, 0x00, 0x07, 0, 0};
    uint8_t expected[] = {0x01, 0x86, 0x03, 0, 0};
    add_crc(req, sizeof req);
    add_crc(expected, sizeof expected);
    size_t len = modbus_rtu_handle(&regmap, 1, req, sizeof req, resp);
    CHECK_BYTES(resp, len, expected);
    CHECK_EQ_HEX(regmap_set.led_mode, 2); /* unchanged */
}

int main(void)
{
    /* after start-up */
    test_default_settings();
    test_measurements_invalid_at_start();
    /* reading */
    test_read_values();
    test_every_address();
    test_negative_temperature();
    test_32bit_word_order();
    test_firmware_version();
    test_reserved_read_zero();
    test_input_block_edges();
    /* writing settings */
    test_write_setting();
    test_write_negative_offset();
    test_write_limits();
    test_write_all_or_nothing();
    test_holding_edges();
    /* commands */
    test_command();
    test_command_invalid();
    test_command_replaced();
    test_init_resets();
    /* through the parser */
    test_frame_read_temperature();
    test_frame_write_refused();
    return test_summary();
}
