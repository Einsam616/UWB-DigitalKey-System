#include "TM1637Keypad.h"
#include "delay.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

#define BOARD_KEYPAD_CLK_PORT GPIOB
#define BOARD_KEYPAD_CLK_PIN  GPIO_Pin_6
#define BOARD_KEYPAD_DIO_PORT GPIOB
#define BOARD_KEYPAD_DIO_PIN  GPIO_Pin_7
#define TM1637_DISPLAY_DIGITS  4u
#define TM1637_DISPLAY_ON      0x8Fu
#define TM1637_DISPLAY_OFF     0x80u
#define TM1637_SEGMENT_P       0x73u
#define TM1637_SEGMENT_F       0x71u
#define TM1637_SEGMENT_CURSOR  0x08u
#define TM1637_SEGMENT_O       0x3Fu
#define TM1637_SEGMENT_L       0x38u
#define TM1637_SEGMENT_D       0x5Eu
#define TM1637_SEGMENT_N       0x54u
#define TM1637_SEGMENT_E       0x79u
#define TM1637_SEGMENT_U       0x1Cu
#define TM1637_SEGMENT_R       0x50u

typedef struct {
    uint8_t value;
    char name;
} TM1637KeyMap;

static const TM1637KeyMap tm1637_key_map[] = {
    {0xEFu, '1'}, {0x6Fu, '2'}, {0xAFu, '3'}, {0x2Fu, 'A'},
    {0xF7u, '4'}, {0x77u, '5'}, {0xB7u, '6'}, {0x37u, 'B'},
    {0xD7u, '7'}, {0x57u, '8'}, {0x97u, '9'}, {0x17u, 'C'},
    {0xCFu, '*'}, {0x4Fu, '0'}, {0x8Fu, '#'}, {0x0Fu, 'D'}
};

/* 与参考工程 LED_table 的 0~9 段码完全一致。 */
static const uint8_t tm1637_digit_segments[10] = {
    0x3Fu, 0x06u, 0x5Bu, 0x4Fu, 0x66u,
    0x6Du, 0x7Du, 0x07u, 0x7Fu, 0x6Fu
};

static uint8_t s_display_enabled;

// 将键盘时钟线置为高电平
static void TM1637Keypad_ClkHigh(void)
{
    GPIO_SetBits(BOARD_KEYPAD_CLK_PORT, BOARD_KEYPAD_CLK_PIN);
}

// 将键盘时钟线置为低电平
static void TM1637Keypad_ClkLow(void)
{
    GPIO_ResetBits(BOARD_KEYPAD_CLK_PORT, BOARD_KEYPAD_CLK_PIN);
}

// 将键盘数据线置为高电平
static void TM1637Keypad_DioHigh(void)
{
    GPIO_SetBits(BOARD_KEYPAD_DIO_PORT, BOARD_KEYPAD_DIO_PIN);
}

// 将键盘数据线置为低电平
static void TM1637Keypad_DioLow(void)
{
    GPIO_ResetBits(BOARD_KEYPAD_DIO_PORT, BOARD_KEYPAD_DIO_PIN);
}

// 将数据线配置为推挽输出。参考工程使用推挽方式，能够保证模块无外部上拉时的高电平。
static void TM1637Keypad_DioOutput(void)
{
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin = BOARD_KEYPAD_DIO_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(BOARD_KEYPAD_DIO_PORT, &gpio);
}

// 将数据线配置为上拉输入
static void TM1637Keypad_DioInput(void)
{
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin = BOARD_KEYPAD_DIO_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(BOARD_KEYPAD_DIO_PORT, &gpio);
}

// 读取键盘数据线电平
static uint8_t TM1637Keypad_DioRead(void)
{
    return GPIO_ReadInputDataBit(BOARD_KEYPAD_DIO_PORT,
                                 BOARD_KEYPAD_DIO_PIN);
}

// 发送 TM1637 起始时序
static void TM1637Keypad_Start(void)
{
    TM1637Keypad_DioOutput();
    TM1637Keypad_ClkHigh();
    TM1637Keypad_DioHigh();
    delay_us(2u);
    TM1637Keypad_DioLow();
}

// 发送 TM1637 停止时序
static void TM1637Keypad_Stop(void)
{
    TM1637Keypad_DioOutput();
    TM1637Keypad_ClkLow();
    TM1637Keypad_DioLow();
    delay_us(2u);
    TM1637Keypad_ClkHigh();
    delay_us(2u);
    TM1637Keypad_DioHigh();
}

// 按低位在前方式写入一个字节
static void TM1637Keypad_WriteByte(uint8_t data)
{
    uint8_t index;

    TM1637Keypad_DioOutput();
    for (index = 0u; index < 8u; index++) {
        TM1637Keypad_ClkLow();
        if ((data & 0x01u) != 0u) {
            TM1637Keypad_DioHigh();
        } else {
            TM1637Keypad_DioLow();
        }
        delay_us(3u);
        data >>= 1;
        TM1637Keypad_ClkHigh();
        delay_us(3u);
    }
}

// 完成一个应答时钟周期
static void TM1637Keypad_WaitAck(void)
{
    TM1637Keypad_ClkLow();
    TM1637Keypad_DioHigh();
    TM1637Keypad_DioInput();
    delay_us(5u);
    TM1637Keypad_ClkHigh();
    delay_us(2u);
    TM1637Keypad_ClkLow();
    TM1637Keypad_DioOutput();
    TM1637Keypad_DioHigh();
}

/**
 * @brief 将四个段码写入 TM1637 显示寄存器。
 * @param segments 从左到右的四个共阳数码管段码。
 * @note 写入过程约 0.3 ms，不改变键盘扫描映射。
 */
static void TM1637Keypad_WriteDisplay(const uint8_t segments[TM1637_DISPLAY_DIGITS])
{
    uint8_t index;

    TM1637Keypad_Start();
    TM1637Keypad_WriteByte(0x40u);
    TM1637Keypad_WaitAck();
    TM1637Keypad_Stop();

    TM1637Keypad_Start();
    TM1637Keypad_WriteByte(0xC0u);
    TM1637Keypad_WaitAck();
    for (index = 0u; index < TM1637_DISPLAY_DIGITS; ++index)
    {
        TM1637Keypad_WriteByte(segments[index]);
        TM1637Keypad_WaitAck();
    }
    TM1637Keypad_Stop();

    TM1637Keypad_Start();
    TM1637Keypad_WriteByte((s_display_enabled != 0u) ?
                           TM1637_DISPLAY_ON : TM1637_DISPLAY_OFF);
    TM1637Keypad_WaitAck();
    TM1637Keypad_Stop();
}

// 将键值编码转换为按键字符
static char TM1637Keypad_ValueToName(uint8_t key_value)
{
    uint8_t index;
    uint8_t count = (uint8_t)(sizeof(tm1637_key_map) /
                              sizeof(tm1637_key_map[0]));

    for (index = 0u; index < count; index++) {
        if (tm1637_key_map[index].value == key_value) {
            return tm1637_key_map[index].name;
        }
    }
    return TM1637_KEYPAD_NO_KEY;
}

// 初始化 PA4 时钟线和 PA5 数据线
void TM1637Keypad_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    gpio.GPIO_Pin = BOARD_KEYPAD_CLK_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(BOARD_KEYPAD_CLK_PORT, &gpio);
    TM1637Keypad_DioOutput();

    TM1637Keypad_ClkHigh();
    TM1637Keypad_DioHigh();
    s_display_enabled = 1u;
    TM1637Keypad_ClearDisplay();
}

/** @brief 开启或关闭显示控制位，不改变数码管段码和按键扫描。 */
void TM1637Keypad_SetDisplayEnabled(uint8_t enable)
{
    s_display_enabled = (enable != 0u) ? 1u : 0u;
    TM1637Keypad_Start();
    TM1637Keypad_WriteByte((s_display_enabled != 0u) ?
                           TM1637_DISPLAY_ON : TM1637_DISPLAY_OFF);
    TM1637Keypad_WaitAck();
    TM1637Keypad_Stop();
}

/** @brief 在数码管中部显示 P 或 F，其他字符按空白处理。 */
void TM1637Keypad_ShowStatus(char status)
{
    uint8_t segments[TM1637_DISPLAY_DIGITS] = {0u, 0u, 0u, 0u};

    if (status == 'P') segments[1] = TM1637_SEGMENT_P;
    else if (status == 'F') segments[1] = TM1637_SEGMENT_F;
    TM1637Keypad_WriteDisplay(segments);
}

/** @brief 显示已输入数字，下一位输入位置的底横段作为闪烁光标。 */
void TM1637Keypad_ShowInputDigits(const uint8_t digits[TM1637_DISPLAY_DIGITS],
                                  uint8_t count,
                                  uint8_t cursor_on)
{
    uint8_t segments[TM1637_DISPLAY_DIGITS] = {0u, 0u, 0u, 0u};
    uint8_t index;

    if (count > TM1637_DISPLAY_DIGITS) count = TM1637_DISPLAY_DIGITS;
    if (digits == 0) count = 0u;
    for (index = 0u; index < count; ++index)
    {
        if (digits[index] <= 9u)
            segments[index] = tm1637_digit_segments[digits[index]];
    }
    if (count < TM1637_DISPLAY_DIGITS && cursor_on != 0u)
    {
        segments[count] |= TM1637_SEGMENT_CURSOR;
    }
    TM1637Keypad_WriteDisplay(segments);
}

/** @brief 显示 OLD、nEu 或 rE，区分原密码、新密码和再次确认。 */
void TM1637Keypad_ShowPrompt(char prompt)
{
    uint8_t segments[TM1637_DISPLAY_DIGITS] = {0u, 0u, 0u, 0u};

    if (prompt == 'O')
    {
        segments[0] = TM1637_SEGMENT_O;
        segments[1] = TM1637_SEGMENT_L;
        segments[2] = TM1637_SEGMENT_D;
    }
    else if (prompt == 'N')
    {
        segments[0] = TM1637_SEGMENT_N;
        segments[1] = TM1637_SEGMENT_E;
        segments[2] = TM1637_SEGMENT_U;
    }
    else if (prompt == 'R')
    {
        segments[0] = TM1637_SEGMENT_R;
        segments[1] = TM1637_SEGMENT_E;
    }
    TM1637Keypad_WriteDisplay(segments);
}

/** @brief 清空四位数码管。 */
void TM1637Keypad_ClearDisplay(void)
{
    uint8_t segments[TM1637_DISPLAY_DIGITS] = {0u, 0u, 0u, 0u};

    TM1637Keypad_WriteDisplay(segments);
}

// 读取一次键盘原始按键
char TM1637Keypad_ReadKey(void)
{
    uint8_t index;
    uint8_t key_value = 0u;

    TM1637Keypad_Start();
    TM1637Keypad_WriteByte(0x42u);
    TM1637Keypad_WaitAck();

    TM1637Keypad_DioHigh();
    TM1637Keypad_DioInput();
    for (index = 0u; index < 8u; index++) {
        TM1637Keypad_ClkLow();
        delay_us(40u);
        TM1637Keypad_ClkHigh();
        key_value |= (uint8_t)(TM1637Keypad_DioRead() << (7u - index));
        delay_us(40u);
    }

    TM1637Keypad_DioOutput();
    TM1637Keypad_DioHigh();
    TM1637Keypad_WaitAck();
    TM1637Keypad_Stop();

    if (key_value == 0xFFu) {
        return TM1637_KEYPAD_NO_KEY;
    }
    return TM1637Keypad_ValueToName(key_value);
}

// 消抖并且每次按下只上报一个按键字符
char TM1637Keypad_GetPressedKey(void)
{
    static char last_raw = TM1637_KEYPAD_NO_KEY;
    static char reported_key = TM1637_KEYPAD_NO_KEY;
    static uint8_t stable_count;
    char raw = TM1637Keypad_ReadKey();

    if (raw != last_raw) {
        last_raw = raw;
        stable_count = 0u;
        return TM1637_KEYPAD_NO_KEY;
    }
    if (stable_count < 2u) {
        stable_count++;
        return TM1637_KEYPAD_NO_KEY;
    }
    if (raw == TM1637_KEYPAD_NO_KEY) {
        reported_key = TM1637_KEYPAD_NO_KEY;
        return TM1637_KEYPAD_NO_KEY;
    }
    if (reported_key == raw) {
        return TM1637_KEYPAD_NO_KEY;
    }

    reported_key = raw;
    return raw;
}
