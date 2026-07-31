#ifndef DEBUG_SERIAL_H
#define DEBUG_SERIAL_H

#include <stdint.h>

/**
 * @brief Initialize USART1 on PA9/PA10 for polling debug output.
 * @param baud_rate Standard baud rate such as 115200.
 */
void DebugSerial_Init(uint32_t baud_rate);

/**
 * @brief Write one byte and wait for the USART data register.
 * @param value Byte to send.
 */
void DebugSerial_WriteByte(uint8_t value);

/**
 * @brief Write a zero-terminated ASCII string.
 * @param text String to send; null is ignored.
 */
void DebugSerial_WriteText(const char *text);

#endif
