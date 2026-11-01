/**
 * @file    regmap.c
 * @brief   The register map. STUB: filled in by step 3.
 *
 * For now all three slots are NULL. The parser treats a NULL callback as
 * "this device has no registers of this kind" and answers every request
 * with exception 01 (illegal function). So even this empty map gives the
 * correct Modbus behaviour for a device without registers.
 */
#include "regmap.h"

#include <stddef.h>

/* Step 3: write regmap_read_input(), regmap_read_holding() and
 * regmap_write_holding(), and put their addresses in these slots. */
const modbus_regs_t regmap = {
    .read_input = NULL,
    .read_holding = NULL,
    .write_holding = NULL,
};
