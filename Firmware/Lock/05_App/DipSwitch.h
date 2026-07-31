#ifndef DIP_SWITCH_H
#define DIP_SWITCH_H

#include <stdint.h>

/**
 * @brief Initialize the lock-side four-bit identity switch inputs.
 * @note SW1..SW4 use PA6, PA7, PB9, PB10 and map to ID bits 3..0.
 *       Each input uses an internal pull-up; an ON switch connected to GND is 1.
 */
void DipSwitch_Init(void);

/**
 * @brief Read the identity currently selected on the lock-side DIP switch.
 * @return Four-bit value from 0 to 15, with SW1 as the most significant bit.
 */
uint8_t DipSwitch_ReadValue(void);

#endif
