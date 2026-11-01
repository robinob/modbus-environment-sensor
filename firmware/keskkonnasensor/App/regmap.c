/**
 * @file    regmap.c
 * @brief   The register map: turns register addresses into our data.
 *
 * Three callbacks for the parser (read_input, read_holding,
 * write_holding), plus the data they work on. See regmap.h for the data
 * flow and docs/registers.md for the table.
 */
#include "regmap.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "version.h"

regmap_inputs_t regmap_in;
regmap_settings_t regmap_set;

/* The last command written to register 100, waiting for app.c. */
static uint16_t pending_command;

/* ---- Holding register limits --------------------------------------------- */

/* The allowed range and the default of each setting, one row per register.
 * Signed registers arrive as uint16_t on the wire; is_signed says to read
 * them as int16_t (two's complement) before comparing with min/max.
 * int32_t holds both -1000 and 65000, so one type fits every row. */
typedef struct {
    int32_t min;
    int32_t max;
    int32_t def;
    bool is_signed;
} setting_limits_t;

static const setting_limits_t limits[REGMAP_HOLDING_COUNT] = {
    /*  min     max    default  signed */
    {    1,   3600,      10,    false }, /* 0 interval_s     */
    { -1000,  1000,       0,    true  }, /* 1 temp_offset    */
    { -1000,  1000,       0,    true  }, /* 2 hum_offset     */
    {    0,      2,       2,    false }, /* 3 led_mode       */
    {    0,    100,      50,    false }, /* 4 led_brightness */
    {  400,  65000,    1000,    false }, /* 5 eco2_yellow    */
    {  400,  65000,    1500,    false }, /* 6 eco2_red       */
    {    0,      1,       1,    false }, /* 7 led_activity   */
};

/* Is this raw register value allowed in holding register addr? */
static bool setting_in_range(uint16_t addr, uint16_t raw)
{
    const setting_limits_t *l = &limits[addr];
    int32_t v = l->is_signed ? (int32_t)(int16_t)raw : (int32_t)raw;
    return v >= l->min && v <= l->max;
}

/* Read setting addr as a raw register value. */
static uint16_t setting_get(uint16_t addr)
{
    switch (addr) {
    case 0: return regmap_set.interval_s;
    case 1: return (uint16_t)regmap_set.temp_offset;
    case 2: return (uint16_t)regmap_set.hum_offset;
    case 3: return regmap_set.led_mode;
    case 4: return regmap_set.led_brightness;
    case 5: return regmap_set.eco2_yellow;
    case 6: return regmap_set.eco2_red;
    case 7: return regmap_set.led_activity;
    default: return 0u;
    }
}

/* Store a raw register value into setting addr. Already range-checked. */
static void setting_set(uint16_t addr, uint16_t raw)
{
    switch (addr) {
    case 0: regmap_set.interval_s = raw; break;
    case 1: regmap_set.temp_offset = (int16_t)raw; break;
    case 2: regmap_set.hum_offset = (int16_t)raw; break;
    case 3: regmap_set.led_mode = raw; break;
    case 4: regmap_set.led_brightness = raw; break;
    case 5: regmap_set.eco2_yellow = raw; break;
    case 6: regmap_set.eco2_red = raw; break;
    case 7: regmap_set.led_activity = raw; break;
    default: break;
    }
}

/* ---- Input registers ------------------------------------------------------ */

/* The value of input register addr. Reserved addresses read as 0.
 * 32-bit values take two registers, high half first ("big-endian word
 * order"), the most common convention among Modbus devices. */
static uint16_t input_get(uint16_t addr)
{
    const regmap_inputs_t *in = &regmap_in;

    switch (addr) {
    case 0:  return (uint16_t)in->temperature; /* two's complement */
    case 1:  return in->humidity;
    case 2:  return (uint16_t)in->dew_point;
    case 3:  return in->abs_humidity;
    case 4:  return in->aqi_uba;
    case 5:  return in->tvoc_ppb;
    case 6:  return in->eco2_ppm;
    case 7:  return in->ens160_validity;
    case 10: return in->status;
    case 11: return in->sht45_age_s;
    case 12: return in->ens160_age_s;
    case 13: return (uint16_t)(in->uptime_s >> 16);
    case 14: return (uint16_t)(in->uptime_s & 0xFFFFu);
    case 15: return in->cnt_requests;
    case 16: return in->cnt_crc_errors;
    case 17: return in->cnt_sht45_err;
    case 18: return in->cnt_ens160_err;
    case 20: return (uint16_t)((FW_VERSION_MAJOR << 8) | FW_VERSION_MINOR);
    case 21: return in->slave_address;
    case 22: return (uint16_t)(in->sht45_serial >> 16);
    case 23: return (uint16_t)(in->sht45_serial & 0xFFFFu);
    case 24: return in->ens160_part_id;
    case 25: return in->ens160_fw;
    case 26: return in->ens160_fw_rel;
    case 30: return in->ens160_raw[0];
    case 31: return in->ens160_raw[1];
    case 32: return in->ens160_raw[2];
    case 33: return in->ens160_raw[3];
    default: return 0u; /* reserved: 8, 9, 19, 27, 28, 29 */
    }
}

/* ---- The three callbacks -------------------------------------------------- */

static modbus_exception_t read_input(uint16_t start, uint16_t count, uint16_t *values)
{
    if ((uint32_t)start + count > REGMAP_INPUT_COUNT) {
        return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
    }
    for (uint16_t i = 0; i < count; i++) {
        values[i] = input_get((uint16_t)(start + i));
    }
    return MODBUS_EX_NONE;
}

static modbus_exception_t read_holding(uint16_t start, uint16_t count, uint16_t *values)
{
    /* The command register reads as 0: commands are actions, not values. */
    if (start == REGMAP_CMD_ADDRESS && count == 1u) {
        values[0] = 0u;
        return MODBUS_EX_NONE;
    }
    if ((uint32_t)start + count > REGMAP_HOLDING_COUNT) {
        return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
    }
    for (uint16_t i = 0; i < count; i++) {
        values[i] = setting_get((uint16_t)(start + i));
    }
    return MODBUS_EX_NONE;
}

static modbus_exception_t write_holding(uint16_t start, uint16_t count,
                                        const uint16_t *values)
{
    if (start == REGMAP_CMD_ADDRESS && count == 1u) {
        if (values[0] < 1u || values[0] > REGMAP_CMD_LAST) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
        pending_command = values[0];
        return MODBUS_EX_NONE;
    }
    if ((uint32_t)start + count > REGMAP_HOLDING_COUNT) {
        return MODBUS_EX_ILLEGAL_DATA_ADDRESS;
    }
    /* All or nothing (see modbus_write_fn): check every value first... */
    for (uint16_t i = 0; i < count; i++) {
        if (!setting_in_range((uint16_t)(start + i), values[i])) {
            return MODBUS_EX_ILLEGAL_DATA_VALUE;
        }
    }
    /* ...and only then change anything. */
    for (uint16_t i = 0; i < count; i++) {
        setting_set((uint16_t)(start + i), values[i]);
    }
    return MODBUS_EX_NONE;
}

const modbus_regs_t regmap = {
    .read_input = read_input,
    .read_holding = read_holding,
    .write_holding = write_holding,
};

/* ---- Public functions ----------------------------------------------------- */

void regmap_init(void)
{
    /* Everything to 0 first: counters, ages, status, serial number... */
    memset(&regmap_in, 0, sizeof regmap_in);

    /* ...then every measurement to "not available yet". */
    regmap_in.temperature = REGMAP_INVALID_S16;
    regmap_in.humidity = REGMAP_INVALID_U16;
    regmap_in.dew_point = REGMAP_INVALID_S16;
    regmap_in.abs_humidity = REGMAP_INVALID_U16;
    regmap_in.aqi_uba = REGMAP_INVALID_U16;
    regmap_in.tvoc_ppb = REGMAP_INVALID_U16;
    regmap_in.eco2_ppm = REGMAP_INVALID_U16;
    regmap_in.ens160_validity = REGMAP_INVALID_U16;
    regmap_in.sht45_age_s = REGMAP_INVALID_U16;
    regmap_in.ens160_age_s = REGMAP_INVALID_U16;
    for (size_t i = 0; i < 4u; i++) {
        regmap_in.ens160_raw[i] = REGMAP_INVALID_U16;
    }

    /* Settings to their defaults, from the limits table. */
    for (uint16_t addr = 0; addr < REGMAP_HOLDING_COUNT; addr++) {
        setting_set(addr, (uint16_t)limits[addr].def);
    }

    pending_command = REGMAP_CMD_NONE;
}

uint16_t regmap_take_command(void)
{
    uint16_t cmd = pending_command;
    pending_command = REGMAP_CMD_NONE;
    return cmd;
}
