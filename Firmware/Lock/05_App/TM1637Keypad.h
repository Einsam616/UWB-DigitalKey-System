#ifndef __TM1637_KEYPAD_H
#define __TM1637_KEYPAD_H

#include <stdint.h>

#define TM1637_KEYPAD_NO_KEY  ((char)0)

void TM1637Keypad_Init(void);
char TM1637Keypad_ReadKey(void);
char TM1637Keypad_GetPressedKey(void);

/** @brief 开启或关闭四位数码管显示，关闭时保留段码缓存和键盘扫描。 */
void TM1637Keypad_SetDisplayEnabled(uint8_t enable);

/** @brief 在四位数码管中部显示单个状态字符，目前支持 P 和 F。 */
void TM1637Keypad_ShowStatus(char status);

/**
 * @brief 用四位数码管显示已输入的数字和闪烁光标。
 * @param digits 从左到右的数字数组，每项范围 0 到 9。
 * @param count 已输入位数，范围 0 到 4。
 * @param cursor_on 非零时点亮下一位输入位置的底横段光标；输入满四位时不显示光标。
 */
void TM1637Keypad_ShowInputDigits(const uint8_t digits[4],
                                  uint8_t count,
                                  uint8_t cursor_on);

/**
 * @brief 显示密码阶段提示。
 * @param prompt O=原密码，N=新密码，R=再次输入，其他值清屏。
 */
void TM1637Keypad_ShowPrompt(char prompt);

/** @brief 清空四位数码管，显示功能保持开启。 */
void TM1637Keypad_ClearDisplay(void);

#endif
