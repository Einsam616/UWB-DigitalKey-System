#ifndef DIP_SWITCH_H
#define DIP_SWITCH_H

#include <stdint.h>

/**
 * @brief Initialize the key-side four-bit identity switch inputs.
 * @note SW1..SW4 use PB12..PB15 and map to ID bits 3..0.
 *       Each input uses an internal pull-up; an ON switch connected to GND is 1.
 */
void DipSwitch_Init(void);

/**
 * @brief Read the four switch states in their displayed left-to-right order.
 * @param bits Output array containing SW1..SW4 as four values of 0 or 1.
 */
void DipSwitch_ReadBits(uint8_t bits[4]);

/**
 * @brief Pack SW1..SW4 into the transmitted four-bit identity.
 * @return Four-bit value from 0 to 15, with SW1 as the most significant bit.
 */
uint8_t DipSwitch_ReadValue(void);

#endif
