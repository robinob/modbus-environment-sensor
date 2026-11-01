/**
 * @file    regmap.h
 * @brief   The register map: which Modbus registers this board has.
 *
 * The parser (modbus_rtu.c) does not know what any register means. It
 * calls the functions in `regmap` to read or write them. This file is the
 * only place that decides "input register 0 is the temperature".
 *
 * Portable: no HAL calls, so it can be unit-tested on the PC (step 3).
 */
#ifndef REGMAP_H
#define REGMAP_H

#include "modbus_rtu.h"

/* `extern` declares a variable without creating it: "a modbus_regs_t
 * called regmap exists, in some .c file". The real one is created once,
 * in regmap.c. Every file that includes this header can then use it. */
extern const modbus_regs_t regmap;

#endif /* REGMAP_H */
