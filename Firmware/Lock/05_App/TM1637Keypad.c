#include "TM1637Keypad.h"
#include "delay.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

#define BOARD_KEYPAD_CLK_PORT GPIOB
#define BOARD_KEYPAD_CLK_PIN  GPIO_Pin_6
#define BOARD_KEYPAD_DIO_PORT GPIOB
#define BOARD_KEYPAD_DIO_PIN  GPIO_Pin_7

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

// 将数据线配置为开漏输出
static void TM1637Keypad_DioOutput(void)
{
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin = BOARD_KEYPAD_DIO_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
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
