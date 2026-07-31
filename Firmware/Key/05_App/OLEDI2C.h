#ifndef OLED_I2C_H
#define OLED_I2C_H

#include <stdint.h>

#define OLED_I2C_DEGREE_CHAR ((char)0x7F)

typedef enum
{
    OLED_I2C_OK = 0,
    OLED_I2C_NOT_FOUND = -1,
    OLED_I2C_NOT_INITIALIZED = -2,
    OLED_I2C_ARGUMENT = -3
} OLEDI2CStatus;

/**
 * @brief 初始化 PA4/PA6 软件 I2C OLED，并探测 0x3C/0x3D。
 * @return 成功返回 0，两个候选地址都无应答返回负值。
 * @note PA4 为 SCL，PA6 为 SDA；目标为最常见的 128x64 SSD1306。
 */
OLEDI2CStatus OLEDI2C_Init(void);

/**
 * @brief 查询 OLED 是否已经通过 I2C 应答并完成初始化。
 * @return 已就绪返回 1，否则返回 0。
 */
uint8_t OLEDI2C_IsReady(void);

/**
 * @brief 获取探测到的 7 位 I2C 地址。
 * @return 0x3C、0x3D；尚未探测到时返回 0。
 */
uint8_t OLEDI2C_GetAddress(void);

/**
 * @brief 清空 128x64 显存。
 * @return 成功返回 0。
 */
OLEDI2CStatus OLEDI2C_Clear(void);

/**
 * @brief 在指定页显示一个 ASCII 字符。
 * @param x 横坐标，范围 0..122，每个字符占 6 列。
 * @param page 页坐标，范围 0..7。
 * @param character 0x20~0x7E ASCII 字符；OLED_I2C_DEGREE_CHAR 绘制度数符号。
 * @return 成功返回 0。
 */
OLEDI2CStatus OLEDI2C_ShowChar(uint8_t x, uint8_t page, char character);

/**
 * @brief 在指定页显示 ASCII 字符串。
 * @param x 横坐标，范围 0..122。
 * @param page 页坐标，范围 0..7。
 * @param text 以 '\0' 结尾的字符串。
 * @return 成功返回 0。
 * @note 超出右边界的字符会被截断，不会写出显存范围。
 */
OLEDI2CStatus OLEDI2C_ShowString(uint8_t x, uint8_t page, const char *text);

#endif
