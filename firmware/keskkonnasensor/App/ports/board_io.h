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

#endif /* BOARD_IO_H */
