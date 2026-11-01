/**
 * @file    board_io.h
 * @brief   The simple board hardware: DIP switches (and later the LED).
 *
 * In ports/ because it touches the hardware through the HAL.
 */
#ifndef BOARD_IO_H
#define BOARD_IO_H

#include <stdint.h>

/**
 * @brief  Read the Modbus slave address from the DIP switches.
 * @return The address, 1..16.
 */
uint8_t board_read_address(void);

/**
 * @brief  Milliseconds since start-up (the HAL's 1 ms SysTick counter).
 *
 * Wraps around to 0 after 2^32 ms, about 49.7 days. Compute durations as
 * `now - earlier` with uint32_t: unsigned subtraction gives the right
 * answer even across the wrap.
 */
uint32_t board_millis(void);

#endif /* BOARD_IO_H */
