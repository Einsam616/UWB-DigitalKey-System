#ifndef __TM1637_KEYPAD_H
#define __TM1637_KEYPAD_H

#define TM1637_KEYPAD_NO_KEY  ((char)0)

void TM1637Keypad_Init(void);
char TM1637Keypad_ReadKey(void);
char TM1637Keypad_GetPressedKey(void);

#endif
