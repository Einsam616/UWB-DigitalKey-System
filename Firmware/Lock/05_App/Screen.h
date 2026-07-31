#ifndef __SCREEN_H__
#define __SCREEN_H__

#include <stdint.h>

/*
==============================================================================
  STM32 / GD32 软件 SPI 彩屏通用库
==============================================================================
  【这个库是做什么的】
  1. 用普通 GPIO 模拟 SPI，驱动常见 ILI9341 / ST7789 一类彩屏。
  2. 一个 Screen.c + 一个 Screen.h，复制到别的 Keil 工程也能用。
  3. 不是 HAL 库，是标准外设库风格：
     - STM32F1 标准库：stm32f10x.h
     - STM32F4 标准库：stm32f4xx.h
     - GD32F4 标准库：gd32f4xx.h

  【怎么切换平台】
  下面“用户配置区”里有三个开关，只能有一个是 1。

  例如当前是 STM32F103：
      SCREEN_USE_STM32F10X = 1
      SCREEN_USE_STM32F4XX = 0
      SCREEN_USE_GD32F4XX  = 0

  如果要切到梁山派 GD32F450：
      SCREEN_USE_STM32F10X = 0
      SCREEN_USE_STM32F4XX = 0
      SCREEN_USE_GD32F4XX  = 1

  【当前 Lock 接线】
      BLK  -> PB0
      SPI2 SCK/MISO/MOSI -> PB13/PB14/PB15
      CS1/CS2 -> PA8/PA15
      DC/RES -> PA12/PA11
==============================================================================
*/

/* ==========================================================
   用户配置区：一般只改这里
   ========================================================== */

/* 1. 选择平台：只允许一个为 1，另外两个必须为 0 */
#ifndef SCREEN_USE_STM32F10X
#define SCREEN_USE_STM32F10X  1   /* STM32F103 / STM32F1 标准库 */
#endif
#ifndef SCREEN_USE_STM32F4XX
#define SCREEN_USE_STM32F4XX  0   /* STM32F407 / STM32F4 标准库 */
#endif
#ifndef SCREEN_USE_GD32F4XX
#define SCREEN_USE_GD32F4XX   0   /* GD32F450 / 梁山派工程 */
#endif

/* 2. 屏幕大小。横屏常用 320x240，竖屏常用 240x320 */
#define SCREEN_WIDTH          320u
#define SCREEN_HEIGHT         240u

/* 3. 屏幕方向寄存器。
      0x68：横屏方向
      0x48：竖屏方向
      如果显示上下/左右反了，优先改这里。 */
#define SCREEN_MADCTL_VALUE   0xE8u

/* 4. 延时函数。
      0：软延时，最笨但不占用 SysTick。
      1：SysTick 延时，默认推荐，三个平台都能用，精度比软延时好。
      2：使用工程已有的 delay_ms / Delay_ms。

      注意：
      - LCD 初始化时才会用到这个延时，默认 SysTick 模式一般没问题。
      - 如果你的工程已经用 SysTick 做系统节拍或 RTOS，请改成 2，调用工程自己的延时。
      - 如果改成 2，请同步修改下面两行。 */
#ifndef SCREEN_DELAY_MODE
#define SCREEN_DELAY_MODE          2
#endif
#ifndef SCREEN_DELAY_HEADER
#define SCREEN_DELAY_HEADER        "delay.h"
#endif
#ifndef SCREEN_DELAY_MS
#define SCREEN_DELAY_MS(ms)        delay_ms(ms)
#endif

/* STM32F103 fast software SPI path from the verified 2025 display project. */
#ifndef SCREEN_USE_FAST_SOFT_SPI
#define SCREEN_USE_FAST_SOFT_SPI   1
#endif

/* Keep the public library's software-SPI transport on the SPI2 pin group. */
#ifndef SCREEN_USE_HARDWARE_SPI2
#define SCREEN_USE_HARDWARE_SPI2   0
#endif

/* 5. STM32F1 特殊选项。
      STM32F1 使用 PA15 / PB3 / PB4 时，默认被 JTAG 占用。
      如果你确实把背光接到 PA15，就把这里改成 1。 */
#define SCREEN_F10X_DISABLE_JTAG   1

/* 6. 屏幕引脚。
      如果换了接线，只改 PORT 和 PIN。
      注意：PIN 写 SCREEN_PIN_x，别直接写 GPIO_Pin_x / GPIO_PIN_x。 */
#define SCREEN_BLK_PORT       GPIOB
#define SCREEN_BLK_PIN        SCREEN_PIN_0

#define SCREEN_SCK_PORT       GPIOB
#define SCREEN_SCK_PIN        SCREEN_PIN_13

#define SCREEN_MISO_PORT      GPIOB
#define SCREEN_MISO_PIN       SCREEN_PIN_14

#define SCREEN_MOSI_PORT      GPIOB
#define SCREEN_MOSI_PIN       SCREEN_PIN_15

#define SCREEN_CS_PORT        GPIOA
#define SCREEN_CS_PIN         SCREEN_PIN_8

#define SCREEN_CS2_PORT       GPIOA
#define SCREEN_CS2_PIN        SCREEN_PIN_15

/*
 * LCD chip-select routing probe:
 *   1 -> CS1 / PA8 (normal wiring-table interpretation)
 *   2 -> CS2 / PA15 (second diagnostic image)
 *   3 -> CS1 / PA8 and CS2 / PA15 together
 * The unselected device is kept high for every LCD transfer.
 */
#ifndef SCREEN_LCD_CS_INDEX
#define SCREEN_LCD_CS_INDEX   1
#endif

#if ((SCREEN_LCD_CS_INDEX != 1) && (SCREEN_LCD_CS_INDEX != 2) && \
     (SCREEN_LCD_CS_INDEX != 3))
#error "SCREEN_LCD_CS_INDEX must be 1, 2, or 3"
#endif

#define SCREEN_DC_PORT        GPIOA
#define SCREEN_DC_PIN         SCREEN_PIN_12

#define SCREEN_RST_PORT       GPIOA
#define SCREEN_RST_PIN        SCREEN_PIN_11

/* 7. 用到了哪些 GPIO 端口，就把对应端口时钟设为 1。
      默认接线用到了 A/C/D，所以 A/C/D 是 1。
      例如你把某个线改到 PB5，就要把 SCREEN_USE_GPIOB_CLOCK 改成 1。 */
#define SCREEN_USE_GPIOA_CLOCK  1
#define SCREEN_USE_GPIOB_CLOCK  1
#define SCREEN_USE_GPIOC_CLOCK  0
#define SCREEN_USE_GPIOD_CLOCK  0
#define SCREEN_USE_GPIOE_CLOCK  0
#define SCREEN_USE_GPIOF_CLOCK  0
#define SCREEN_USE_GPIOG_CLOCK  0
#define SCREEN_USE_GPIOH_CLOCK  0

/* ==========================================================
   平台适配区：下面一般不用改
   ========================================================== */

#if ((SCREEN_USE_STM32F10X + SCREEN_USE_STM32F4XX + SCREEN_USE_GD32F4XX) != 1)
#error "平台选择错误：SCREEN_USE_STM32F10X / SCREEN_USE_STM32F4XX / SCREEN_USE_GD32F4XX 只能有一个为 1"
#endif

#if (SCREEN_USE_STM32F10X == 1)
#include "stm32f10x.h"
#elif (SCREEN_USE_STM32F4XX == 1)
#include "stm32f4xx.h"
#else
#include "gd32f4xx.h"
#endif

#if (SCREEN_DELAY_MODE == 2)
#include SCREEN_DELAY_HEADER
#endif

#if (SCREEN_USE_GD32F4XX == 1)
    #define SCREEN_PIN_0   GPIO_PIN_0
    #define SCREEN_PIN_1   GPIO_PIN_1
    #define SCREEN_PIN_2   GPIO_PIN_2
    #define SCREEN_PIN_3   GPIO_PIN_3
    #define SCREEN_PIN_4   GPIO_PIN_4
    #define SCREEN_PIN_5   GPIO_PIN_5
    #define SCREEN_PIN_6   GPIO_PIN_6
    #define SCREEN_PIN_7   GPIO_PIN_7
    #define SCREEN_PIN_8   GPIO_PIN_8
    #define SCREEN_PIN_9   GPIO_PIN_9
    #define SCREEN_PIN_10  GPIO_PIN_10
    #define SCREEN_PIN_11  GPIO_PIN_11
    #define SCREEN_PIN_12  GPIO_PIN_12
    #define SCREEN_PIN_13  GPIO_PIN_13
    #define SCREEN_PIN_14  GPIO_PIN_14
    #define SCREEN_PIN_15  GPIO_PIN_15
#else
    #define SCREEN_PIN_0   GPIO_Pin_0
    #define SCREEN_PIN_1   GPIO_Pin_1
    #define SCREEN_PIN_2   GPIO_Pin_2
    #define SCREEN_PIN_3   GPIO_Pin_3
    #define SCREEN_PIN_4   GPIO_Pin_4
    #define SCREEN_PIN_5   GPIO_Pin_5
    #define SCREEN_PIN_6   GPIO_Pin_6
    #define SCREEN_PIN_7   GPIO_Pin_7
    #define SCREEN_PIN_8   GPIO_Pin_8
    #define SCREEN_PIN_9   GPIO_Pin_9
    #define SCREEN_PIN_10  GPIO_Pin_10
    #define SCREEN_PIN_11  GPIO_Pin_11
    #define SCREEN_PIN_12  GPIO_Pin_12
    #define SCREEN_PIN_13  GPIO_Pin_13
    #define SCREEN_PIN_14  GPIO_Pin_14
    #define SCREEN_PIN_15  GPIO_Pin_15
#endif

/* ==========================================================
   常用 RGB565 颜色
   ========================================================== */
#define SCREEN_BLACK    0x0000u
#define SCREEN_NAVY     0x000Fu
#define SCREEN_DGREEN   0x03E0u
#define SCREEN_DCYAN    0x03EFu
#define SCREEN_MAROON   0x7800u
#define SCREEN_PURPLE   0x780Fu
#define SCREEN_OLIVE    0x7BE0u
#define SCREEN_LGRAY    0xC618u
#define SCREEN_DGRAY    0x7BEFu
#define SCREEN_BLUE     0x001Fu
#define SCREEN_GREEN    0x07E0u
#define SCREEN_CYAN     0x07FFu
#define SCREEN_RED      0xF800u
#define SCREEN_MAGENTA  0xF81Fu
#define SCREEN_YELLOW   0xFFE0u
#define SCREEN_WHITE    0xFFFFu
#define SCREEN_ORANGE   0xFD20u

/**
  * @brief  初始化屏幕 GPIO 和 LCD 控制器。
  * @note   使用任何绘图函数前必须先调用本函数。
  */
void Screen_Init(void);

/**
  * @brief  控制背光。
  * @param  enable: 1 打开背光，0 关闭背光。
  */
void Screen_BackLight(uint8_t enable);

/**
  * @brief  用单色清空整屏。
  * @param  color: RGB565 颜色，例如 SCREEN_BLACK。
  */
void Screen_Clear(uint16_t color);

/**
  * @brief  画一个点。
  * @param  x, y : 像素坐标。
  * @param  color: RGB565 颜色。
  */
void Screen_DrawPoint(uint16_t x, uint16_t y, uint16_t color);

/**
  * @brief  填充矩形区域。
  * @param  x, y          : 左上角坐标。
  * @param  width, height : 宽度和高度。
  * @param  color         : RGB565 颜色。
  */
void Screen_FillRect(uint16_t x, uint16_t y,
                     uint16_t width, uint16_t height,
                     uint16_t color);

/**
  * @brief  画空心矩形。
  */
void Screen_DrawRect(uint16_t x, uint16_t y,
                     uint16_t width, uint16_t height,
                     uint16_t color);

/**
  * @brief  画直线。
  */
void Screen_DrawLine(uint16_t x0, uint16_t y0,
                     uint16_t x1, uint16_t y1,
                     uint16_t color);

/**
  * @brief  画空心圆。
  */
void Screen_DrawCircle(uint16_t x, uint16_t y,
                       uint16_t radius, uint16_t color);

/**
  * @brief  画实心圆。
  */
void Screen_FillCircle(uint16_t x, uint16_t y,
                       uint16_t radius, uint16_t color);

/**
  * @brief  显示一个 5x7 ASCII 字符，可放大显示。
  * @param  size: 放大倍数，1 为 5x7 原始大小，建议 1~4。
  */
void Screen_ShowChar(uint16_t x, uint16_t y,
                     char ch,
                     uint16_t color,
                     uint16_t back_color,
                     uint8_t size);

/**
  * @brief  显示 ASCII 字符串。
  * @note   本函数自带英文字库；如需显示中文，请在上层 UI 增加中文字模。
  */
void Screen_ShowString(uint16_t x, uint16_t y,
                       const char *str,
                       uint16_t color,
                       uint16_t back_color,
                       uint8_t size);

void Screen_ShowChar16(uint16_t x, uint16_t y,
                       char ch,
                       uint16_t color,
                       uint16_t back_color,
                       uint8_t scale);

void Screen_ShowString16(uint16_t x, uint16_t y,
                         const char *str,
                         uint16_t color,
                         uint16_t back_color,
                         uint8_t scale);

/*
==============================================================================
  代码示例
==============================================================================

  int main(void)
  {
      Screen_Init();
      Screen_Clear(SCREEN_BLACK);

      Screen_ShowString(20, 20, "HELLO STM32", SCREEN_GREEN,
                        SCREEN_BLACK, 2);
      Screen_DrawRect(10, 60, 120, 60, SCREEN_WHITE);
      Screen_FillCircle(200, 120, 30, SCREEN_RED);

      while (1) {
      }
  }
==============================================================================
*/

#endif
