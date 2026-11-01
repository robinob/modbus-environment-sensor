/**
 * @file    app.c
 * @brief   The main loop: ties the drivers and the Modbus parser together.
 *
 * The flow of one Modbus request through the firmware:
 *
 *     RS-485 wire
 *        -> mb_uart_receive()    a complete frame arrived    (ports/mb_uart.c)
 *        -> modbus_rtu_handle()  check it, build the reply   (modbus/modbus_rtu.c)
 *             -> regmap          read or write the registers (regmap.c)
 *        -> mb_uart_send()       the reply goes out          (ports/mb_uart.c)
 *
 * This file calls no HAL functions itself. It only talks to small
 * interfaces (mb_uart.h, board_io.h, regmap.h), so it stays easy to read
 * and could later be tested on the PC with fake drivers.
 *
 * Skeleton status: the UART and DIP drivers are still empty stubs. Each
 * stub says which step fills it in.
 */
#include "app.h"

#include <stddef.h>
#include <stdint.h>

#include "board_io.h"
#include "mb_uart.h"
#include "modbus_rtu.h"
#include "regmap.h"

/* Our Modbus slave address, read from the DIP switches once at start-up.
 * Changing the switches therefore needs a reset to take effect. */
static uint8_t own_address;

/* The frame buffers. 256 bytes each, so they are kept here as static
 * variables (in .bss, zeroed at start-up) instead of as local variables:
 * the stack is only 1 KB, and modbus_rtu_handle() already uses about
 * 600 bytes of it. 512 bytes of locals on top would overflow it. */
static uint8_t rx_frame[MODBUS_RTU_MAX_FRAME];
static uint8_t tx_frame[MODBUS_RTU_MAX_FRAME];

/* For the uptime counter: the clock reading at the last update, and the
 * milliseconds collected that do not make a full second yet. */
static uint32_t last_ms;
static uint32_t ms_into_second;

/* Count whole seconds since start-up into regmap_in.uptime_s.
 *
 * Why not just uptime = board_millis() / 1000? Because the millisecond
 * counter wraps to 0 after 49.7 days, and the uptime would jump back with
 * it. Adding up the differences instead keeps counting: `now - last_ms`
 * is computed in unsigned 32-bit arithmetic, which gives the right
 * difference even when the counter has just wrapped (e.g. 5 - 0xFFFFFFFE
 * = 7). */
static void update_uptime(void)
{
    uint32_t now = board_millis();
    ms_into_second += now - last_ms;
    last_ms = now;
    while (ms_into_second >= 1000u) {
        ms_into_second -= 1000u;
        regmap_in.uptime_s++;
    }
}

/* Carry out a command the master wrote into holding register 100. */
static void run_command(uint16_t cmd)
{
    switch (cmd) {
    case REGMAP_CMD_RESET_COUNTERS:
        regmap_in.cnt_requests = 0;
        regmap_in.cnt_crc_errors = 0;
        regmap_in.cnt_sht45_err = 0;
        regmap_in.cnt_ens160_err = 0;
        break;
    case REGMAP_CMD_SHT45_HEATER:
        /* Step 6: ask the SHT45 driver for one heater pulse. */
        break;
    default:
        break;
    }
}

void app_init(void)
{
    regmap_init();

    own_address = board_read_address();
    regmap_in.slave_address = own_address;

    last_ms = board_millis();
    mb_uart_init();

    /* Steps 6-7: start the SHT45 and ENS160 drivers here. */
}

void app_poll(void)
{
    update_uptime();

    /* 1. Has a complete frame arrived? 0 means not yet: nothing to do. */
    size_t rx_len = mb_uart_receive(rx_frame, sizeof rx_frame);

    if (rx_len > 0u) {
        /* 2. Let the parser check it and build the reply. */
        size_t tx_len = modbus_rtu_handle(&regmap, own_address,
                                          rx_frame, rx_len, tx_frame);

        /* 3. Send the reply. 0 means stay silent: damaged frame, another
         * board's frame, or a broadcast. */
        if (tx_len > 0u) {
            mb_uart_send(tx_frame, tx_len);
            regmap_in.cnt_requests++;
        }
    }

    /* 4. A write to the command register is carried out here, outside
     * the parser, so a slow command never delays the Modbus reply. */
    run_command(regmap_take_command());

    /* Steps 6-7: give the sensor drivers a turn here. Each call does one
     * small step (start a measurement, or collect a finished one) and
     * returns at once, so the loop keeps running. */
}
