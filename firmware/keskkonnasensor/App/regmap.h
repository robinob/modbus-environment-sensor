/**
 * @file    regmap.h
 * @brief   The register map: which Modbus registers this board has.
 *
 * The parser (modbus_rtu.c) does not know what any register means. It
 * calls the functions in `regmap` to read or write them. This file is the
 * only place that decides "input register 0 is the temperature".
 * The full table, for whoever programs the master, is docs/registers.md.
 *
 * How the data flows:
 *   - The sensor drivers and app.c WRITE measurements into regmap_in.
 *   - The parser READS them through regmap.read_input.
 *   - The master WRITES settings through regmap.write_holding; they land
 *     in regmap_set, which app.c and the drivers READ.
 *   - A write to the command register is queued; app.c picks it up with
 *     regmap_take_command().
 *
 * Portable: no HAL calls, so it is unit-tested on the PC
 * (tests/test_regmap.c).
 */
#ifndef REGMAP_H
#define REGMAP_H

#include <stdint.h>

#include "modbus_rtu.h"

/* ---- Register counts ---------------------------------------------------- */

/* Input registers 0..33 exist. Reserved ones inside that range read as 0,
 * so a master can read a whole block in one request. 34 and up: error 02. */
#define REGMAP_INPUT_COUNT 34u

/* Holding registers 0..7 are the settings. */
#define REGMAP_HOLDING_COUNT 8u

/* Holding register 100 is the command register (see REGMAP_CMD_...). */
#define REGMAP_CMD_ADDRESS 100u

/* ---- "No valid value" markers -------------------------------------------- */

/* A measurement that is not available (sensor missing, not read yet, or
 * failed) is reported as one of these instead of an error reply, so the
 * master always gets an answer. The status register says why.
 *   - Signed values (temperature, dew point): -32768, 0x8000 on the wire.
 *     Real temperatures never get near it.
 *   - Unsigned values: 65535, 0xFFFF. 0x8000 would not work here, because
 *     32768 ppb is a real possible TVOC value (range 0..65000). */
#define REGMAP_INVALID_S16 INT16_MIN
#define REGMAP_INVALID_U16 0xFFFFu

/* ---- Status register bits (input register 10) ---------------------------- */

#define REGMAP_ST_SHT45_OK       (1u << 0) /* temperature/humidity valid  */
#define REGMAP_ST_ENS160_OK      (1u << 1) /* air quality values valid    */
#define REGMAP_ST_ENS160_WARMUP  (1u << 2) /* first 3 min after power-on  */
#define REGMAP_ST_ENS160_STARTUP (1u << 3) /* first hour of sensor's life */
#define REGMAP_ST_SHT45_FAIL     (1u << 4) /* SHT45 does not answer       */
#define REGMAP_ST_ENS160_FAIL    (1u << 5) /* ENS160 does not answer      */
#define REGMAP_ST_ENS160_COMP    (1u << 6) /* ENS160 gets T/RH from SHT45 */

/* ---- Commands (written to holding register 100) -------------------------- */

#define REGMAP_CMD_NONE           0u
#define REGMAP_CMD_SHT45_HEATER   1u /* one heater pulse, dries condensation */
#define REGMAP_CMD_RESET_COUNTERS 2u /* zero the error/request counters      */
#define REGMAP_CMD_LAST           2u /* highest valid command                */

/* ---- The data ------------------------------------------------------------- */

/**
 * Everything the master can read (input registers). Filled in by app.c
 * and the sensor drivers. The comment gives the register address.
 */
typedef struct {
    int16_t  temperature;     /*  0: degC x 100, offset applied     */
    uint16_t humidity;        /*  1: %RH x 100, offset applied      */
    int16_t  dew_point;       /*  2: degC x 100                     */
    uint16_t abs_humidity;    /*  3: g/m3 x 100                     */
    uint16_t aqi_uba;         /*  4: 1..5                           */
    uint16_t tvoc_ppb;        /*  5: 0..65000                       */
    uint16_t eco2_ppm;        /*  6: 400..65000                     */
    uint16_t ens160_validity; /*  7: 0 ok, 1 warm-up, 2 start-up, 3 invalid */
    uint16_t status;          /* 10: REGMAP_ST_... bits             */
    uint16_t sht45_age_s;     /* 11: seconds since last good read   */
    uint16_t ens160_age_s;    /* 12: seconds since last good read   */
    uint32_t uptime_s;        /* 13-14: seconds since power-on      */
    uint16_t cnt_requests;    /* 15: Modbus requests answered       */
    uint16_t cnt_crc_errors;  /* 16: frames dropped: bad CRC, line error, too long */
    uint16_t cnt_sht45_err;   /* 17: failed SHT45 transfers         */
    uint16_t cnt_ens160_err;  /* 18: failed ENS160 transfers        */
    uint16_t slave_address;   /* 21: from the DIP switches          */
    uint32_t sht45_serial;    /* 22-23                              */
    uint16_t ens160_part_id;  /* 24: 0x0160 for a real ENS160       */
    uint16_t ens160_fw;       /* 25: major x 256 + minor            */
    uint16_t ens160_fw_rel;   /* 26: release                        */
    uint16_t ens160_raw[4];   /* 30-33: raw sensor values, log units */
} regmap_inputs_t;

/**
 * Everything the master can change (holding registers 0..7). Always in
 * range: regmap refuses out-of-range writes with error 03.
 */
typedef struct {
    uint16_t interval_s;      /* 0: measurement interval, 1..3600 s   */
    int16_t  temp_offset;     /* 1: degC x 100, -1000..1000           */
    int16_t  hum_offset;      /* 2: %RH x 100, -1000..1000            */
    uint16_t led_mode;        /* 3: 0 off, 1 status only, 2 air quality */
    uint16_t led_brightness;  /* 4: 0..100 %                          */
    uint16_t eco2_yellow;     /* 5: ppm, LED yellow from here         */
    uint16_t eco2_red;        /* 6: ppm, LED red from here            */
    uint16_t led_activity;    /* 7: 1 = blue flash on Modbus requests */
} regmap_settings_t;

/* The data itself, created in regmap.c. `extern` only says "it exists
 * somewhere"; every file that includes this header can use it. */
extern regmap_inputs_t regmap_in;
extern regmap_settings_t regmap_set;

/* The callbacks for the parser: pass &regmap to modbus_rtu_handle(). */
extern const modbus_regs_t regmap;

/**
 * @brief  Set the defaults: settings to their default values, every
 *         measurement to "invalid", counters to 0. Call once at start-up.
 */
void regmap_init(void);

/**
 * @brief  Take the last command written to register 100, if any.
 * @return REGMAP_CMD_... value, or REGMAP_CMD_NONE. Taking it clears it.
 *
 * Only one command is kept: a new one written before the previous one was
 * taken replaces it.
 */
uint16_t regmap_take_command(void);

#endif /* REGMAP_H */
