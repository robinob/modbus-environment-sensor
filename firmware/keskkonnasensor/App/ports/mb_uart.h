/**
 * @file    mb_uart.h
 * @brief   Modbus RTU over USART1: receive whole frames, send replies.
 *
 * Modbus RTU frames have no length byte and no end marker. A frame ends
 * when the line stays quiet for 3.5 character times (1.75 ms above
 * 19200 baud). This driver collects bytes as they arrive and hands over
 * a frame only once that silence has been seen.
 *
 * The RS-485 chip (MAX13487E) switches between sending and receiving by
 * itself, so there is no direction pin to drive here.
 */
#ifndef MB_UART_H
#define MB_UART_H

#include <stddef.h>
#include <stdint.h>

/** @brief  Start receiving. Call once, after MX_USART1_UART_Init(). */
void mb_uart_init(void);

/**
 * @brief  Take a complete received frame, if there is one.
 *
 * Never waits: returns 0 at once when no complete frame is ready yet.
 *
 * @param  frame  Where to copy the frame.
 * @param  max    Size of that buffer in bytes.
 * @return Length of the frame in bytes, or 0 if none is ready.
 *         This is the req_len that modbus_rtu_handle() needs.
 */
size_t mb_uart_receive(uint8_t *frame, size_t max);

/**
 * @brief  Send a reply frame.
 * @param  frame  The bytes, CRC included.
 * @param  len    How many bytes to send.
 */
void mb_uart_send(const uint8_t *frame, size_t len);

#endif /* MB_UART_H */
