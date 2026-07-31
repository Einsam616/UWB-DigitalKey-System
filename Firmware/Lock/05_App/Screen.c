#include "Screen.h"

#if (SCREEN_USE_STM32F10X == 1)
#include "stm32f10x_rcc.h"
#if (SCREEN_USE_HARDWARE_SPI2 == 1)
#include "stm32f10x_spi.h"
#endif
#endif

/* ==========================================================
   LCD 常用指令
   ========================================================== */
#define SCREEN_CMD_SLEEP_OUT       0x11u
#define SCREEN_CMD_COLUMN_SET      0x2Au
#define SCREEN_CMD_PAGE_SET        0x2Bu
#define SCREEN_CMD_MEMORY_WRITE    0x2Cu
#define SCREEN_CMD_MEMORY_ACCESS   0x36u
#define SCREEN_CMD_PIXEL_FORMAT    0x3Au
#define SCREEN_CMD_DISPLAY_ON      0x29u

#if (SCREEN_USE_GD32F4XX == 1)
typedef uint32_t ScreenGpioPort;
#else
typedef GPIO_TypeDef *ScreenGpioPort;
#endif

/* 5x7 ASCII 字库：每个字符 5 列，每列低 7 位对应从上到下的像素。 */
static const uint8_t Screen_Font5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /*   */
    {0x00,0x00,0x5F,0x00,0x00}, /* ! */
    {0x00,0x07,0x00,0x07,0x00}, /* " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* $ */
    {0x23,0x13,0x08,0x64,0x62}, /* % */
    {0x36,0x49,0x55,0x22,0x50}, /* & */
    {0x00,0x05,0x03,0x00,0x00}, /* ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* ) */
    {0x14,0x08,0x3E,0x08,0x14}, /* * */
    {0x08,0x08,0x3E,0x08,0x08}, /* + */
    {0x00,0x50,0x30,0x00,0x00}, /* , */
    {0x08,0x08,0x08,0x08,0x08}, /* - */
    {0x00,0x60,0x60,0x00,0x00}, /* . */
    {0x20,0x10,0x08,0x04,0x02}, /* / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* : */
    {0x00,0x56,0x36,0x00,0x00}, /* ; */
    {0x08,0x14,0x22,0x41,0x00}, /* < */
    {0x14,0x14,0x14,0x14,0x14}, /* = */
    {0x00,0x41,0x22,0x14,0x08}, /* > */
    {0x02,0x01,0x51,0x09,0x06}, /* ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x09,0x01}, /* F */
    {0x3E,0x41,0x49,0x49,0x7A}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x3F,0x40,0x38,0x40,0x3F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x07,0x08,0x70,0x08,0x07}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
    {0x00,0x7F,0x41,0x41,0x00}, /* [ */
    {0x02,0x04,0x08,0x10,0x20}, /* \ */
    {0x00,0x41,0x41,0x7F,0x00}, /* ] */
    {0x04,0x02,0x01,0x02,0x04}, /* ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* _ */
    {0x00,0x01,0x02,0x04,0x00}, /* ` */
    {0x20,0x54,0x54,0x54,0x78}, /* a */
    {0x7F,0x48,0x44,0x44,0x38}, /* b */
    {0x38,0x44,0x44,0x44,0x20}, /* c */
    {0x38,0x44,0x44,0x48,0x7F}, /* d */
    {0x38,0x54,0x54,0x54,0x18}, /* e */
    {0x08,0x7E,0x09,0x01,0x02}, /* f */
    {0x0C,0x52,0x52,0x52,0x3E}, /* g */
    {0x7F,0x08,0x04,0x04,0x78}, /* h */
    {0x00,0x44,0x7D,0x40,0x00}, /* i */
    {0x20,0x40,0x44,0x3D,0x00}, /* j */
    {0x7F,0x10,0x28,0x44,0x00}, /* k */
    {0x00,0x41,0x7F,0x40,0x00}, /* l */
    {0x7C,0x04,0x18,0x04,0x78}, /* m */
    {0x7C,0x08,0x04,0x04,0x78}, /* n */
    {0x38,0x44,0x44,0x44,0x38}, /* o */
    {0x7C,0x14,0x14,0x14,0x08}, /* p */
    {0x08,0x14,0x14,0x18,0x7C}, /* q */
    {0x7C,0x08,0x04,0x04,0x08}, /* r */
    {0x48,0x54,0x54,0x54,0x20}, /* s */
    {0x04,0x3F,0x44,0x40,0x20}, /* t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* w */
    {0x44,0x28,0x10,0x28,0x44}, /* x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* y */
    {0x44,0x64,0x54,0x4C,0x44}, /* z */
    {0x00,0x08,0x36,0x41,0x00}, /* { */
    {0x00,0x00,0x7F,0x00,0x00}, /* | */
    {0x00,0x41,0x36,0x08,0x00}, /* } */
    {0x10,0x08,0x08,0x10,0x08}  /* ~ */
};

#if ((SCREEN_DELAY_MODE == 0) || (SCREEN_DELAY_MODE == 1))
static void Screen_SoftDelayMs(uint32_t ms)
{
    volatile uint32_t i;

    while (ms-- > 0u) {
        /*
         * 简易毫秒延时，只用于 LCD 复位和退出睡眠。
         * 不依赖 SysTick，适合做通用库的兜底方案。
         */
        for (i = 0u; i < 9000u; i++) {
        }
    }
}
#endif

#if (SCREEN_DELAY_MODE == 1)
static void Screen_SysTickDelayMs(uint32_t ms)
{
    uint32_t ticks;

    if (SystemCoreClock == 0u) {
        SystemCoreClockUpdate();
    }

    ticks = SystemCoreClock / 1000u;
    if (ticks == 0u) {
        Screen_SoftDelayMs(ms);
        return;
    }

    while (ms-- > 0u) {
        /*
         * SysTick 是 Cortex-M 内核自带定时器，STM32F1 / STM32F4 / GD32F4 都有。
         * 这里每次只延时 1ms，LOAD 不会超过 24 位限制。
         */
        SysTick->LOAD = ticks - 1u;
        SysTick->VAL = 0u;
        SysTick->CTRL = 0x00000005u;
        while ((SysTick->CTRL & 0x00010000u) == 0u) {
        }
        SysTick->CTRL = 0x00000004u;
    }
}
#endif

static void Screen_DelayMs(uint32_t ms)
{
#if (SCREEN_DELAY_MODE == 0)
    Screen_SoftDelayMs(ms);
#elif (SCREEN_DELAY_MODE == 1)
    Screen_SysTickDelayMs(ms);
#elif (SCREEN_DELAY_MODE == 2)
    SCREEN_DELAY_MS(ms);
#else
    #error "SCREEN_DELAY_MODE 只能设置为 0、1、2"
#endif
}

static void Screen_EnableGpioClock(void)
{
#if (SCREEN_USE_STM32F10X == 1)
    #if (SCREEN_USE_GPIOA_CLOCK == 1)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    #endif
    #if (SCREEN_USE_GPIOB_CLOCK == 1)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    #endif
    #if (SCREEN_USE_GPIOC_CLOCK == 1)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    #endif
    #if (SCREEN_USE_GPIOD_CLOCK == 1)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
    #endif
    #if (SCREEN_USE_GPIOE_CLOCK == 1)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);
    #endif
    #if (SCREEN_USE_GPIOF_CLOCK == 1)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOF, ENABLE);
    #endif
    #if (SCREEN_USE_GPIOG_CLOCK == 1)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOG, ENABLE);
    #endif
    #if (SCREEN_F10X_DISABLE_JTAG == 1)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
    #endif
    #if (SCREEN_USE_HARDWARE_SPI2 == 1)
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    #endif
#elif (SCREEN_USE_STM32F4XX == 1)
    #if (SCREEN_USE_GPIOA_CLOCK == 1)
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    #endif
    #if (SCREEN_USE_GPIOB_CLOCK == 1)
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    #endif
    #if (SCREEN_USE_GPIOC_CLOCK == 1)
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    #endif
    #if (SCREEN_USE_GPIOD_CLOCK == 1)
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    #endif
    #if (SCREEN_USE_GPIOE_CLOCK == 1)
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    #endif
    #if (SCREEN_USE_GPIOF_CLOCK == 1)
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);
    #endif
    #if (SCREEN_USE_GPIOG_CLOCK == 1)
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOG, ENABLE);
    #endif
    #if (SCREEN_USE_GPIOH_CLOCK == 1)
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOH, ENABLE);
    #endif
#else
    #if (SCREEN_USE_GPIOA_CLOCK == 1)
    rcu_periph_clock_enable(RCU_GPIOA);
    #endif
    #if (SCREEN_USE_GPIOB_CLOCK == 1)
    rcu_periph_clock_enable(RCU_GPIOB);
    #endif
    #if (SCREEN_USE_GPIOC_CLOCK == 1)
    rcu_periph_clock_enable(RCU_GPIOC);
    #endif
    #if (SCREEN_USE_GPIOD_CLOCK == 1)
    rcu_periph_clock_enable(RCU_GPIOD);
    #endif
    #if (SCREEN_USE_GPIOE_CLOCK == 1)
    rcu_periph_clock_enable(RCU_GPIOE);
    #endif
    #if (SCREEN_USE_GPIOF_CLOCK == 1)
    rcu_periph_clock_enable(RCU_GPIOF);
    #endif
    #if (SCREEN_USE_GPIOG_CLOCK == 1)
    rcu_periph_clock_enable(RCU_GPIOG);
    #endif
    #if (SCREEN_USE_GPIOH_CLOCK == 1)
    rcu_periph_clock_enable(RCU_GPIOH);
    #endif
#endif
}

static void Screen_GpioOutputInit(ScreenGpioPort port, uint32_t pin)
{
#if (SCREEN_USE_STM32F10X == 1)
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin = (uint16_t)pin;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(port, &gpio);
#elif (SCREEN_USE_STM32F4XX == 1)
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin = pin;
    gpio.GPIO_Mode = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(port, &gpio);
#else
    gpio_mode_set(port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, pin);
    gpio_output_options_set(port, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, pin);
#endif
}

static void Screen_PinHigh(ScreenGpioPort port, uint32_t pin)
{
#if (SCREEN_USE_GD32F4XX == 1)
    gpio_bit_set(port, pin);
#else
    GPIO_SetBits(port, (uint16_t)pin);
#endif
}

static void Screen_PinLow(ScreenGpioPort port, uint32_t pin)
{
#if (SCREEN_USE_GD32F4XX == 1)
    gpio_bit_reset(port, pin);
#else
    GPIO_ResetBits(port, (uint16_t)pin);
#endif
}

#if ((SCREEN_USE_STM32F10X == 1) && (SCREEN_USE_HARDWARE_SPI2 == 1))
static void Screen_HardwareSpiInit(void)
{
    GPIO_InitTypeDef gpio;
    SPI_InitTypeDef spi;

    gpio.GPIO_Pin = (uint16_t)(SCREEN_SCK_PIN | SCREEN_MOSI_PIN);
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SCREEN_SCK_PORT, &gpio);

    gpio.GPIO_Pin = (uint16_t)SCREEN_MISO_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(SCREEN_MISO_PORT, &gpio);

    SPI_I2S_DeInit(SPI2);
    SPI_StructInit(&spi);
    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Master;
    spi.SPI_DataSize = SPI_DataSize_8b;
    spi.SPI_CPOL = SPI_CPOL_Low;
    spi.SPI_CPHA = SPI_CPHA_1Edge;
    spi.SPI_NSS = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;
    spi.SPI_FirstBit = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial = 7u;
    SPI_Init(SPI2, &spi);
    SPI_Cmd(SPI2, ENABLE);
}
#endif

#if ((SCREEN_USE_STM32F10X == 1) && (SCREEN_USE_FAST_SOFT_SPI == 1) && \
     (SCREEN_USE_HARDWARE_SPI2 == 0))
    #define SCREEN_F10X_PIN_LOW(port, pin)  ((port)->BRR = (uint32_t)(pin))
    #define SCREEN_F10X_PIN_HIGH(port, pin) ((port)->BSRR = (uint32_t)(pin))
    #if (SCREEN_LCD_CS_INDEX == 1)
        #define SCREEN_CS_LOW()       SCREEN_F10X_PIN_LOW(SCREEN_CS_PORT, SCREEN_CS_PIN)
        #define SCREEN_CS_HIGH()      SCREEN_F10X_PIN_HIGH(SCREEN_CS_PORT, SCREEN_CS_PIN)
        #define SCREEN_AUX_CS_HIGH()  SCREEN_F10X_PIN_HIGH(SCREEN_CS2_PORT, SCREEN_CS2_PIN)
    #elif (SCREEN_LCD_CS_INDEX == 2)
        #define SCREEN_CS_LOW()       SCREEN_F10X_PIN_LOW(SCREEN_CS2_PORT, SCREEN_CS2_PIN)
        #define SCREEN_CS_HIGH()      SCREEN_F10X_PIN_HIGH(SCREEN_CS2_PORT, SCREEN_CS2_PIN)
        #define SCREEN_AUX_CS_HIGH()  SCREEN_F10X_PIN_HIGH(SCREEN_CS_PORT, SCREEN_CS_PIN)
    #else
        #define SCREEN_CS_LOW()       do { \
            SCREEN_F10X_PIN_LOW(SCREEN_CS_PORT, SCREEN_CS_PIN); \
            SCREEN_F10X_PIN_LOW(SCREEN_CS2_PORT, SCREEN_CS2_PIN); \
        } while (0)
        #define SCREEN_CS_HIGH()      do { \
            SCREEN_F10X_PIN_HIGH(SCREEN_CS_PORT, SCREEN_CS_PIN); \
            SCREEN_F10X_PIN_HIGH(SCREEN_CS2_PORT, SCREEN_CS2_PIN); \
        } while (0)
        #define SCREEN_AUX_CS_HIGH()  do { } while (0)
    #endif
    #define SCREEN_DC_LOW()       SCREEN_F10X_PIN_LOW(SCREEN_DC_PORT, SCREEN_DC_PIN)
    #define SCREEN_DC_HIGH()      SCREEN_F10X_PIN_HIGH(SCREEN_DC_PORT, SCREEN_DC_PIN)
    #define SCREEN_RST_LOW()      SCREEN_F10X_PIN_LOW(SCREEN_RST_PORT, SCREEN_RST_PIN)
    #define SCREEN_RST_HIGH()     SCREEN_F10X_PIN_HIGH(SCREEN_RST_PORT, SCREEN_RST_PIN)
    #define SCREEN_SCK_LOW()      SCREEN_F10X_PIN_LOW(SCREEN_SCK_PORT, SCREEN_SCK_PIN)
    #define SCREEN_SCK_HIGH()     SCREEN_F10X_PIN_HIGH(SCREEN_SCK_PORT, SCREEN_SCK_PIN)
    #define SCREEN_MOSI_LOW()     SCREEN_F10X_PIN_LOW(SCREEN_MOSI_PORT, SCREEN_MOSI_PIN)
    #define SCREEN_MOSI_HIGH()    SCREEN_F10X_PIN_HIGH(SCREEN_MOSI_PORT, SCREEN_MOSI_PIN)
#else
    #if (SCREEN_LCD_CS_INDEX == 1)
        #define SCREEN_CS_LOW()       Screen_PinLow(SCREEN_CS_PORT, SCREEN_CS_PIN)
        #define SCREEN_CS_HIGH()      Screen_PinHigh(SCREEN_CS_PORT, SCREEN_CS_PIN)
        #define SCREEN_AUX_CS_HIGH()  Screen_PinHigh(SCREEN_CS2_PORT, SCREEN_CS2_PIN)
    #elif (SCREEN_LCD_CS_INDEX == 2)
        #define SCREEN_CS_LOW()       Screen_PinLow(SCREEN_CS2_PORT, SCREEN_CS2_PIN)
        #define SCREEN_CS_HIGH()      Screen_PinHigh(SCREEN_CS2_PORT, SCREEN_CS2_PIN)
        #define SCREEN_AUX_CS_HIGH()  Screen_PinHigh(SCREEN_CS_PORT, SCREEN_CS_PIN)
    #else
        #define SCREEN_CS_LOW()       do { \
            Screen_PinLow(SCREEN_CS_PORT, SCREEN_CS_PIN); \
            Screen_PinLow(SCREEN_CS2_PORT, SCREEN_CS2_PIN); \
        } while (0)
        #define SCREEN_CS_HIGH()      do { \
            Screen_PinHigh(SCREEN_CS_PORT, SCREEN_CS_PIN); \
            Screen_PinHigh(SCREEN_CS2_PORT, SCREEN_CS2_PIN); \
        } while (0)
        #define SCREEN_AUX_CS_HIGH()  do { } while (0)
    #endif
    #define SCREEN_DC_LOW()       Screen_PinLow(SCREEN_DC_PORT, SCREEN_DC_PIN)
    #define SCREEN_DC_HIGH()      Screen_PinHigh(SCREEN_DC_PORT, SCREEN_DC_PIN)
    #define SCREEN_RST_LOW()      Screen_PinLow(SCREEN_RST_PORT, SCREEN_RST_PIN)
    #define SCREEN_RST_HIGH()     Screen_PinHigh(SCREEN_RST_PORT, SCREEN_RST_PIN)
    #define SCREEN_SCK_LOW()      Screen_PinLow(SCREEN_SCK_PORT, SCREEN_SCK_PIN)
    #define SCREEN_SCK_HIGH()     Screen_PinHigh(SCREEN_SCK_PORT, SCREEN_SCK_PIN)
    #define SCREEN_MOSI_LOW()     Screen_PinLow(SCREEN_MOSI_PORT, SCREEN_MOSI_PIN)
    #define SCREEN_MOSI_HIGH()    Screen_PinHigh(SCREEN_MOSI_PORT, SCREEN_MOSI_PIN)
#endif

#if defined(__CC_ARM)
    #define SCREEN_FORCE_INLINE static __forceinline
#elif defined(__GNUC__)
    #define SCREEN_FORCE_INLINE static inline __attribute__((always_inline))
#else
    #define SCREEN_FORCE_INLINE static inline
#endif

static void Screen_GpioInit(void)
{
    Screen_EnableGpioClock();

    Screen_GpioOutputInit(SCREEN_BLK_PORT, SCREEN_BLK_PIN);
#if ((SCREEN_USE_STM32F10X == 1) && (SCREEN_USE_HARDWARE_SPI2 == 1))
    Screen_HardwareSpiInit();
#else
    Screen_GpioOutputInit(SCREEN_SCK_PORT, SCREEN_SCK_PIN);
    Screen_GpioOutputInit(SCREEN_MOSI_PORT, SCREEN_MOSI_PIN);
#endif
    Screen_GpioOutputInit(SCREEN_CS_PORT, SCREEN_CS_PIN);
    Screen_GpioOutputInit(SCREEN_CS2_PORT, SCREEN_CS2_PIN);
    Screen_GpioOutputInit(SCREEN_DC_PORT, SCREEN_DC_PIN);
    Screen_GpioOutputInit(SCREEN_RST_PORT, SCREEN_RST_PIN);

    SCREEN_CS_HIGH();
    SCREEN_AUX_CS_HIGH();
#if !((SCREEN_USE_STM32F10X == 1) && (SCREEN_USE_HARDWARE_SPI2 == 1))
    SCREEN_SCK_HIGH();
    SCREEN_MOSI_HIGH();
#endif
    Screen_BackLight(0u);
}

SCREEN_FORCE_INLINE void Screen_WriteByte(uint8_t data)
{
#if ((SCREEN_USE_STM32F10X == 1) && (SCREEN_USE_HARDWARE_SPI2 == 1))
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET) {
    }
    SPI_I2S_SendData(SPI2, data);
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET) {
    }
    (void)SPI_I2S_ReceiveData(SPI2);
#elif ((SCREEN_USE_STM32F10X == 1) && (SCREEN_USE_FAST_SOFT_SPI == 1))
    #define SCREEN_WRITE_BIT(mask)         \
        do {                               \
            SCREEN_SCK_LOW();              \
            if ((data & (mask)) != 0u) {   \
                SCREEN_MOSI_HIGH();         \
            } else {                       \
                SCREEN_MOSI_LOW();          \
            }                              \
            SCREEN_SCK_HIGH();             \
        } while (0)

    SCREEN_WRITE_BIT(0x80u);
    SCREEN_WRITE_BIT(0x40u);
    SCREEN_WRITE_BIT(0x20u);
    SCREEN_WRITE_BIT(0x10u);
    SCREEN_WRITE_BIT(0x08u);
    SCREEN_WRITE_BIT(0x04u);
    SCREEN_WRITE_BIT(0x02u);
    SCREEN_WRITE_BIT(0x01u);

    #undef SCREEN_WRITE_BIT
#else
    uint8_t i;

    for (i = 0u; i < 8u; i++) {
        SCREEN_SCK_LOW();
        if ((data & 0x80u) != 0u) {
            SCREEN_MOSI_HIGH();
        } else {
            SCREEN_MOSI_LOW();
        }
        SCREEN_SCK_HIGH();
        data <<= 1;
    }
#endif
}

static void Screen_WriteCommand(uint8_t command)
{
    SCREEN_CS_LOW();
    SCREEN_DC_LOW();
    Screen_WriteByte(command);
    SCREEN_CS_HIGH();
}

static void Screen_WriteData8(uint8_t data)
{
    SCREEN_CS_LOW();
    SCREEN_DC_HIGH();
    Screen_WriteByte(data);
    SCREEN_CS_HIGH();
}

static void Screen_WriteData16(uint16_t data)
{
    SCREEN_CS_LOW();
    SCREEN_DC_HIGH();
    Screen_WriteByte((uint8_t)(data >> 8));
    Screen_WriteByte((uint8_t)data);
    SCREEN_CS_HIGH();
}

static void Screen_WriteColorStream(uint16_t color, uint32_t count)
{
    SCREEN_CS_LOW();
    SCREEN_DC_HIGH();
    while (count-- > 0u) {
        Screen_WriteByte((uint8_t)(color >> 8));
        Screen_WriteByte((uint8_t)color);
    }
    SCREEN_CS_HIGH();
}

static void Screen_Reset(void)
{
    SCREEN_RST_HIGH();
    Screen_DelayMs(20u);
    SCREEN_RST_LOW();
    Screen_DelayMs(80u);
    SCREEN_RST_HIGH();
    Screen_DelayMs(120u);
}

static void Screen_SetWindow(uint16_t x0, uint16_t y0,
                             uint16_t x1, uint16_t y1)
{
    Screen_WriteCommand(SCREEN_CMD_COLUMN_SET);
    Screen_WriteData16(x0);
    Screen_WriteData16(x1);

    Screen_WriteCommand(SCREEN_CMD_PAGE_SET);
    Screen_WriteData16(y0);
    Screen_WriteData16(y1);

    Screen_WriteCommand(SCREEN_CMD_MEMORY_WRITE);
}

static void Screen_DrawPointSafe(int16_t x, int16_t y, uint16_t color)
{
    if ((x < 0) || (y < 0)) {
        return;
    }
    Screen_DrawPoint((uint16_t)x, (uint16_t)y, color);
}

void Screen_Init(void)
{
    Screen_GpioInit();
    Screen_Reset();

    Screen_WriteCommand(SCREEN_CMD_SLEEP_OUT);
    Screen_DelayMs(120u);

    Screen_WriteCommand(SCREEN_CMD_MEMORY_ACCESS);
    Screen_WriteData8(SCREEN_MADCTL_VALUE);

    Screen_WriteCommand(SCREEN_CMD_PIXEL_FORMAT);
    Screen_WriteData8(0x55u);

    Screen_WriteCommand(SCREEN_CMD_DISPLAY_ON);
    Screen_DelayMs(20u);
}

void Screen_BackLight(uint8_t enable)
{
    if (enable != 0u) {
        Screen_PinHigh(SCREEN_BLK_PORT, SCREEN_BLK_PIN);
    } else {
        Screen_PinLow(SCREEN_BLK_PORT, SCREEN_BLK_PIN);
    }
}

void Screen_Clear(uint16_t color)
{
    Screen_FillRect(0u, 0u, SCREEN_WIDTH, SCREEN_HEIGHT, color);
}

void Screen_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    if ((x >= SCREEN_WIDTH) || (y >= SCREEN_HEIGHT)) {
        return;
    }

    Screen_SetWindow(x, y, x, y);
    Screen_WriteData16(color);
}

void Screen_FillRect(uint16_t x, uint16_t y,
                     uint16_t width, uint16_t height,
                     uint16_t color)
{
    uint32_t count;

    if ((x >= SCREEN_WIDTH) || (y >= SCREEN_HEIGHT) ||
        (width == 0u) || (height == 0u)) {
        return;
    }

    if ((uint32_t)x + width > SCREEN_WIDTH) {
        width = (uint16_t)(SCREEN_WIDTH - x);
    }
    if ((uint32_t)y + height > SCREEN_HEIGHT) {
        height = (uint16_t)(SCREEN_HEIGHT - y);
    }

    Screen_SetWindow(x, y, (uint16_t)(x + width - 1u),
                     (uint16_t)(y + height - 1u));
    count = (uint32_t)width * height;
    Screen_WriteColorStream(color, count);
}

void Screen_DrawRect(uint16_t x, uint16_t y,
                     uint16_t width, uint16_t height,
                     uint16_t color)
{
    if ((width < 2u) || (height < 2u)) {
        return;
    }

    Screen_FillRect(x, y, width, 1u, color);
    Screen_FillRect(x, (uint16_t)(y + height - 1u), width, 1u, color);
    Screen_FillRect(x, y, 1u, height, color);
    Screen_FillRect((uint16_t)(x + width - 1u), y, 1u, height, color);
}

void Screen_DrawLine(uint16_t x0, uint16_t y0,
                     uint16_t x1, uint16_t y1,
                     uint16_t color)
{
    int16_t dx = (x0 > x1) ? (int16_t)(x0 - x1) : (int16_t)(x1 - x0);
    int16_t dy = (y0 > y1) ? (int16_t)(y0 - y1) : (int16_t)(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = (dx > dy ? dx : -dy) / 2;
    int16_t e2;
    int16_t x = (int16_t)x0;
    int16_t y = (int16_t)y0;

    while (1) {
        Screen_DrawPointSafe(x, y, color);
        if ((x == (int16_t)x1) && (y == (int16_t)y1)) {
            break;
        }
        e2 = err;
        if (e2 > -dx) {
            err = (int16_t)(err - dy);
            x = (int16_t)(x + sx);
        }
        if (e2 < dy) {
            err = (int16_t)(err + dx);
            y = (int16_t)(y + sy);
        }
    }
}

void Screen_DrawCircle(uint16_t x, uint16_t y,
                       uint16_t radius, uint16_t color)
{
    int16_t a = 0;
    int16_t b = (int16_t)radius;
    int16_t di = (int16_t)(3 - (radius << 1));
    int16_t cx = (int16_t)x;
    int16_t cy = (int16_t)y;

    while (a <= b) {
        Screen_DrawPointSafe((int16_t)(cx + a), (int16_t)(cy - b), color);
        Screen_DrawPointSafe((int16_t)(cx + b), (int16_t)(cy - a), color);
        Screen_DrawPointSafe((int16_t)(cx + b), (int16_t)(cy + a), color);
        Screen_DrawPointSafe((int16_t)(cx + a), (int16_t)(cy + b), color);
        Screen_DrawPointSafe((int16_t)(cx - a), (int16_t)(cy + b), color);
        Screen_DrawPointSafe((int16_t)(cx - b), (int16_t)(cy + a), color);
        Screen_DrawPointSafe((int16_t)(cx - a), (int16_t)(cy - b), color);
        Screen_DrawPointSafe((int16_t)(cx - b), (int16_t)(cy - a), color);
        if (di < 0) {
            di = (int16_t)(di + 4 * a + 6);
        } else {
            di = (int16_t)(di + 10 + 4 * (a - b));
            b--;
        }
        a++;
    }
}

void Screen_FillCircle(uint16_t x, uint16_t y,
                       uint16_t radius, uint16_t color)
{
    int16_t dx;
    int16_t dy;
    int16_t r = (int16_t)radius;
    int16_t cx = (int16_t)x;
    int16_t cy = (int16_t)y;

    for (dy = (int16_t)-r; dy <= r; dy++) {
        for (dx = (int16_t)-r; dx <= r; dx++) {
            if ((dx * dx + dy * dy) <= (r * r)) {
                Screen_DrawPointSafe((int16_t)(cx + dx),
                                     (int16_t)(cy + dy), color);
            }
        }
    }
}

void Screen_ShowChar(uint16_t x, uint16_t y,
                     char ch,
                     uint16_t color,
                     uint16_t back_color,
                     uint8_t size)
{
    const uint8_t *glyph;
    uint8_t col;
    uint8_t row;
    uint8_t scale_x;
    uint8_t scale_y;
    uint8_t pixel_on;
    uint16_t pixel_color;
    uint16_t char_width;
    uint16_t char_height;

    if (size == 0u) {
        size = 1u;
    }
    if ((ch < ' ') || (ch > '~')) {
        ch = '?';
    }

    glyph = Screen_Font5x7[(uint8_t)ch - 32u];
    char_width = (uint16_t)(6u * size);
    char_height = (uint16_t)(8u * size);
    if ((uint32_t)x + char_width > SCREEN_WIDTH ||
        (uint32_t)y + char_height > SCREEN_HEIGHT) {
        return;
    }

    Screen_SetWindow(x, y,
                     (uint16_t)(x + char_width - 1u),
                     (uint16_t)(y + char_height - 1u));
    SCREEN_CS_LOW();
    SCREEN_DC_HIGH();
    for (row = 0u; row < 8u; row++) {
        for (scale_y = 0u; scale_y < size; scale_y++) {
            for (col = 0u; col < 6u; col++) {
                pixel_on = (col < 5u) ?
                           (uint8_t)((glyph[col] >> row) & 0x01u) : 0u;
                pixel_color = (pixel_on != 0u) ? color : back_color;
                for (scale_x = 0u; scale_x < size; scale_x++) {
                    Screen_WriteByte((uint8_t)(pixel_color >> 8));
                    Screen_WriteByte((uint8_t)pixel_color);
                }
            }
        }
    }
    SCREEN_CS_HIGH();
}

void Screen_ShowString(uint16_t x, uint16_t y,
                       const char *str,
                       uint16_t color,
                       uint16_t back_color,
                       uint8_t size)
{
    uint16_t start_x = x;
    uint16_t cursor_x = x;
    uint16_t cursor_y = y;
    uint16_t char_width;
    uint16_t char_height;

    if (size == 0u) {
        size = 1u;
    }

    char_width = (uint16_t)(6u * size);
    char_height = (uint16_t)(8u * size);

    while ((str != 0) && (*str != '\0')) {
        if (*str == '\r') {
            str++;
            continue;
        }
        if (*str == '\n') {
            cursor_x = start_x;
            cursor_y = (uint16_t)(cursor_y + char_height);
            str++;
            continue;
        }
        if ((uint32_t)cursor_x + char_width > SCREEN_WIDTH) {
            cursor_x = start_x;
            cursor_y = (uint16_t)(cursor_y + char_height);
        }
        if ((uint32_t)cursor_y + char_height > SCREEN_HEIGHT) {
            break;
        }

        Screen_ShowChar(cursor_x, cursor_y, *str,
                        color, back_color, size);
        cursor_x = (uint16_t)(cursor_x + char_width);
        str++;
    }
}

void Screen_ShowChar16(uint16_t x, uint16_t y,
                       char ch,
                       uint16_t color,
                       uint16_t back_color,
                       uint8_t scale)
{
    const uint8_t *glyph;
    uint8_t col;
    uint8_t row;
    uint8_t scale_x;
    uint8_t scale_y;
    uint8_t pixel_on;
    uint16_t pixel_color;
    uint16_t char_width;
    uint16_t char_height;

    if (scale == 0u) scale = 1u;
    if ((ch < ' ') || (ch > '~')) ch = '?';
    char_width = (uint16_t)(6u * scale);
    char_height = (uint16_t)(16u * scale);
    if (((uint32_t)x + char_width > SCREEN_WIDTH) ||
        ((uint32_t)y + char_height > SCREEN_HEIGHT)) return;

    glyph = Screen_Font5x7[(uint8_t)ch - 32u];
    Screen_SetWindow(x, y,
                     (uint16_t)(x + char_width - 1u),
                     (uint16_t)(y + char_height - 1u));
    SCREEN_CS_LOW();
    SCREEN_DC_HIGH();
    for (row = 0u; row < 8u; row++) {
        for (scale_y = 0u; scale_y < (uint8_t)(2u * scale); scale_y++) {
            for (col = 0u; col < 6u; col++) {
                pixel_on = (col < 5u) ?
                           (uint8_t)((glyph[col] >> row) & 0x01u) : 0u;
                pixel_color = (pixel_on != 0u) ? color : back_color;
                for (scale_x = 0u; scale_x < scale; scale_x++) {
                    Screen_WriteByte((uint8_t)(pixel_color >> 8));
                    Screen_WriteByte((uint8_t)pixel_color);
                }
            }
        }
    }
    SCREEN_CS_HIGH();
}

void Screen_ShowString16(uint16_t x, uint16_t y,
                         const char *str,
                         uint16_t color,
                         uint16_t back_color,
                         uint8_t scale)
{
    uint16_t cursor_x = x;
    uint16_t char_width;

    if (scale == 0u) scale = 1u;
    char_width = (uint16_t)(6u * scale);
    while ((str != 0) && (*str != '\0')) {
        if ((uint32_t)cursor_x + char_width > SCREEN_WIDTH) break;
        Screen_ShowChar16(cursor_x, y, *str, color, back_color, scale);
        cursor_x = (uint16_t)(cursor_x + char_width);
        str++;
    }
}
