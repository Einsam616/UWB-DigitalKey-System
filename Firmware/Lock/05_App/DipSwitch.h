#ifndef DIP_SWITCH_H
#define DIP_SWITCH_H

#include <stdint.h>

#define DIP_SWITCH_EVENT_NONE    0x00u
#define DIP_SWITCH_EVENT_CHANGED 0x01u
#define DIP_SWITCH_EVENT_APPLY   0x02u

/*
 * Lock ID switch wiring, in displayed bit order:
 *   SW1 -> PA7  (bit3), SW2 -> PB1  (bit2),
 *   SW3 -> PB10 (bit1), SW4 -> PB11 (bit0).
 * PB9 is the active-low apply button. All inputs use internal pull-ups.
 */
void DipSwitch_Init(void);

/* Debounce the switches and report changed/apply events. */
uint8_t DipSwitch_Service(uint32_t tick_ms);

/* Return the debounced switch value used as the pending ID. */
uint8_t DipSwitch_GetId(void);

#endif
