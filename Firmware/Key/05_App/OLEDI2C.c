#include "OLEDI2C.h"

#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

#include "delay.h"

#define OLED_I2C_ADDRESS_1 0x3Cu
#define OLED_I2C_ADDRESS_2 0x3Du

#define OLED_SCL_PIN GPIO_Pin_4
#define OLED_SDA_PIN GPIO_Pin_6

static uint8_t s_initialized;
static uint8_t s_ready;
static uint8_t s_address;

static void OLEDI2C_SdaOutput(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = OLED_SDA_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);
}

static void OLEDI2C_SdaInput(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = OLED_SDA_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);
}

static void OLEDI2C_Scl(uint8_t high)
{
    GPIO_WriteBit(GPIOA, OLED_SCL_PIN, (high != 0u) ? Bit_SET : Bit_RESET);
}

static void OLEDI2C_Sda(uint8_t high)
{
    GPIO_WriteBit(GPIOA, OLED_SDA_PIN, (high != 0u) ? Bit_SET : Bit_RESET);
}

static uint8_t OLEDI2C_ReadSda(void)
{
    return (GPIO_ReadInputDataBit(GPIOA, OLED_SDA_PIN) != Bit_RESET) ? 1u : 0u;
}

static uint8_t OLEDI2C_Start(void)
{
    OLEDI2C_SdaOutput();
    OLEDI2C_Sda(1u);
    OLEDI2C_Scl(1u);
    delay_us(3u);
    if (OLEDI2C_ReadSda() == 0u)
    {
        return 0u;
    }
    OLEDI2C_Sda(0u);
    delay_us(3u);
    OLEDI2C_Scl(0u);
    return 1u;
}

static void OLEDI2C_Stop(void)
{
    OLEDI2C_SdaOutput();
    OLEDI2C_Scl(0u);
    OLEDI2C_Sda(0u);
    delay_us(2u);
    OLEDI2C_Scl(1u);
    OLEDI2C_Sda(1u);
    delay_us(2u);
}

static uint8_t OLEDI2C_WaitAck(void)
{
    uint8_t retry;
    OLEDI2C_SdaInput();
    OLEDI2C_Sda(1u);
    OLEDI2C_Scl(1u);
    for (retry = 0u; retry < 60u; ++retry)
    {
        if (OLEDI2C_ReadSda() == 0u)
        {
            OLEDI2C_Scl(0u);
            return 1u;
        }
        delay_us(1u);
    }
    OLEDI2C_Scl(0u);
    return 0u;
}

static void OLEDI2C_SendByte(uint8_t value)
{
    uint8_t bit;
    OLEDI2C_SdaOutput();
    for (bit = 0u; bit < 8u; ++bit)
    {
        OLEDI2C_Scl(0u);
        OLEDI2C_Sda((value & 0x80u) != 0u);
        value <<= 1u;
        delay_us(1u);
        OLEDI2C_Scl(1u);
        delay_us(1u);
    }
    OLEDI2C_Scl(0u);
}

static uint8_t OLEDI2C_AddressProbe(uint8_t address)
{
    uint8_t acknowledged = 0u;
    if (OLEDI2C_Start() != 0u)
    {
        OLEDI2C_SendByte((uint8_t)(address << 1u));
        acknowledged = OLEDI2C_WaitAck();
    }
    OLEDI2C_Stop();
    return acknowledged;
}

static OLEDI2CStatus OLEDI2C_Write(uint8_t control,
                                   const uint8_t *data,
                                   uint8_t length)
{
    uint8_t i;
    if (s_initialized == 0u || s_ready == 0u)
    {
        return OLED_I2C_NOT_INITIALIZED;
    }
    if (data == 0 || length == 0u)
    {
        return OLED_I2C_ARGUMENT;
    }
    if (OLEDI2C_Start() == 0u)
    {
        OLEDI2C_Stop();
        return OLED_I2C_NOT_FOUND;
    }
    OLEDI2C_SendByte((uint8_t)(s_address << 1u));
    if (OLEDI2C_WaitAck() == 0u)
    {
        OLEDI2C_Stop();
        return OLED_I2C_NOT_FOUND;
    }
    OLEDI2C_SendByte(control);
    if (OLEDI2C_WaitAck() == 0u)
    {
        OLEDI2C_Stop();
        return OLED_I2C_NOT_FOUND;
    }
    for (i = 0u; i < length; ++i)
    {
        OLEDI2C_SendByte(data[i]);
        if (OLEDI2C_WaitAck() == 0u)
        {
            OLEDI2C_Stop();
            return OLED_I2C_NOT_FOUND;
        }
    }
    OLEDI2C_Stop();
    return OLED_I2C_OK;
}

static OLEDI2CStatus OLEDI2C_Command(uint8_t command)
{
    return OLEDI2C_Write(0x00u, &command, 1u);
}

static uint8_t OLEDI2C_Glyph(char character, uint8_t column)
{
    static const uint8_t font5x7[95][5] =
    {
        {0x00u,0x00u,0x00u,0x00u,0x00u}, {0x00u,0x00u,0x5Fu,0x00u,0x00u},
        {0x00u,0x07u,0x00u,0x07u,0x00u}, {0x14u,0x7Fu,0x14u,0x7Fu,0x14u},
        {0x24u,0x2Au,0x7Fu,0x2Au,0x12u}, {0x23u,0x13u,0x08u,0x64u,0x62u},
        {0x36u,0x49u,0x55u,0x22u,0x50u}, {0x00u,0x05u,0x03u,0x00u,0x00u},
        {0x00u,0x1Cu,0x22u,0x41u,0x00u}, {0x00u,0x41u,0x22u,0x1Cu,0x00u},
        {0x14u,0x08u,0x3Eu,0x08u,0x14u}, {0x08u,0x08u,0x3Eu,0x08u,0x08u},
        {0x00u,0x50u,0x30u,0x00u,0x00u}, {0x08u,0x08u,0x08u,0x08u,0x08u},
        {0x00u,0x60u,0x60u,0x00u,0x00u}, {0x20u,0x10u,0x08u,0x04u,0x02u},
        {0x3Eu,0x51u,0x49u,0x45u,0x3Eu}, {0x00u,0x42u,0x7Fu,0x40u,0x00u},
        {0x42u,0x61u,0x51u,0x49u,0x46u}, {0x21u,0x41u,0x45u,0x4Bu,0x31u},
        {0x18u,0x14u,0x12u,0x7Fu,0x10u}, {0x27u,0x45u,0x45u,0x45u,0x39u},
        {0x3Cu,0x4Au,0x49u,0x49u,0x30u}, {0x01u,0x71u,0x09u,0x05u,0x03u},
        {0x36u,0x49u,0x49u,0x49u,0x36u}, {0x06u,0x49u,0x49u,0x29u,0x1Eu},
        {0x00u,0x36u,0x36u,0x00u,0x00u}, {0x00u,0x56u,0x36u,0x00u,0x00u},
        {0x08u,0x14u,0x22u,0x41u,0x00u}, {0x14u,0x14u,0x14u,0x14u,0x14u},
        {0x00u,0x41u,0x22u,0x14u,0x08u}, {0x02u,0x01u,0x51u,0x09u,0x06u},
        {0x32u,0x49u,0x79u,0x41u,0x3Eu}, {0x7Eu,0x11u,0x11u,0x11u,0x7Eu},
        {0x7Fu,0x49u,0x49u,0x49u,0x36u}, {0x3Eu,0x41u,0x41u,0x41u,0x22u},
        {0x7Fu,0x41u,0x41u,0x22u,0x1Cu}, {0x7Fu,0x49u,0x49u,0x49u,0x41u},
        {0x7Fu,0x09u,0x09u,0x09u,0x01u}, {0x3Eu,0x41u,0x49u,0x49u,0x7Au},
        {0x7Fu,0x08u,0x08u,0x08u,0x7Fu}, {0x00u,0x41u,0x7Fu,0x41u,0x00u},
        {0x20u,0x40u,0x41u,0x3Fu,0x01u}, {0x7Fu,0x08u,0x14u,0x22u,0x41u},
        {0x7Fu,0x40u,0x40u,0x40u,0x40u}, {0x7Fu,0x02u,0x0Cu,0x02u,0x7Fu},
        {0x7Fu,0x04u,0x08u,0x10u,0x7Fu}, {0x3Eu,0x41u,0x41u,0x41u,0x3Eu},
        {0x7Fu,0x09u,0x09u,0x09u,0x06u}, {0x3Eu,0x41u,0x51u,0x21u,0x5Eu},
        {0x7Fu,0x09u,0x19u,0x29u,0x46u}, {0x46u,0x49u,0x49u,0x49u,0x31u},
        {0x01u,0x01u,0x7Fu,0x01u,0x01u}, {0x3Fu,0x40u,0x40u,0x40u,0x3Fu},
        {0x1Fu,0x20u,0x40u,0x20u,0x1Fu}, {0x3Fu,0x40u,0x38u,0x40u,0x3Fu},
        {0x63u,0x14u,0x08u,0x14u,0x63u}, {0x07u,0x08u,0x70u,0x08u,0x07u},
        {0x61u,0x51u,0x49u,0x45u,0x43u}, {0x00u,0x7Fu,0x41u,0x41u,0x00u},
        {0x02u,0x04u,0x08u,0x10u,0x20u}, {0x00u,0x41u,0x41u,0x7Fu,0x00u},
        {0x04u,0x02u,0x01u,0x02u,0x04u}, {0x40u,0x40u,0x40u,0x40u,0x40u},
        {0x00u,0x01u,0x02u,0x04u,0x00u}, {0x20u,0x54u,0x54u,0x54u,0x78u},
        {0x7Fu,0x48u,0x44u,0x44u,0x38u}, {0x38u,0x44u,0x44u,0x44u,0x20u},
        {0x38u,0x44u,0x44u,0x48u,0x7Fu}, {0x38u,0x54u,0x54u,0x54u,0x18u},
        {0x08u,0x7Eu,0x09u,0x01u,0x02u}, {0x0Cu,0x52u,0x52u,0x52u,0x3Eu},
        {0x7Fu,0x08u,0x04u,0x04u,0x78u}, {0x00u,0x44u,0x7Du,0x40u,0x00u},
        {0x20u,0x40u,0x44u,0x3Du,0x00u}, {0x7Fu,0x10u,0x28u,0x44u,0x00u},
        {0x00u,0x41u,0x7Fu,0x40u,0x00u}, {0x7Cu,0x04u,0x18u,0x04u,0x78u},
        {0x7Cu,0x08u,0x04u,0x04u,0x78u}, {0x38u,0x44u,0x44u,0x44u,0x38u},
        {0x7Cu,0x14u,0x14u,0x14u,0x08u}, {0x08u,0x14u,0x14u,0x18u,0x7Cu},
        {0x7Cu,0x08u,0x04u,0x04u,0x08u}, {0x48u,0x54u,0x54u,0x54u,0x20u},
        {0x04u,0x3Fu,0x44u,0x40u,0x20u}, {0x3Cu,0x40u,0x40u,0x20u,0x7Cu},
        {0x1Cu,0x20u,0x40u,0x20u,0x1Cu}, {0x3Cu,0x40u,0x30u,0x40u,0x3Cu},
        {0x44u,0x28u,0x10u,0x28u,0x44u}, {0x0Cu,0x50u,0x50u,0x50u,0x3Cu},
        {0x44u,0x64u,0x54u,0x4Cu,0x44u}, {0x00u,0x08u,0x36u,0x41u,0x00u},
        {0x00u,0x00u,0x7Fu,0x00u,0x00u}, {0x00u,0x41u,0x36u,0x08u,0x00u},
        {0x10u,0x08u,0x08u,0x10u,0x08u}
    };
    static const uint8_t degree[5] = {0x06u, 0x09u, 0x09u, 0x06u, 0x00u};
    uint8_t code = (uint8_t)character;

    if (code == (uint8_t)OLED_I2C_DEGREE_CHAR) return degree[column];
    if (code < 0x20u || code > 0x7Eu) return 0u;
    return font5x7[code - 0x20u][column];
}

static OLEDI2CStatus OLEDI2C_SetPosition(uint8_t x, uint8_t page)
{
    OLEDI2CStatus status;
    status = OLEDI2C_Command((uint8_t)(0xB0u | page));
    if (status != OLED_I2C_OK)
    {
        return status;
    }
    status = OLEDI2C_Command((uint8_t)(0x00u | (x & 0x0Fu)));
    if (status != OLED_I2C_OK)
    {
        return status;
    }
    return OLEDI2C_Command((uint8_t)(0x10u | ((x >> 4u) & 0x0Fu)));
}

/**
 * @brief 初始化 PA4/PA6 软件 I2C OLED。
 * @note PA4 为 SCL，PA6 为 SDA；上电时会探测 0x3C 和 0x3D。
 */
OLEDI2CStatus OLEDI2C_Init(void)
{
    GPIO_InitTypeDef gpio;
    static const uint8_t init_commands[] =
    {
        0xAEu, 0xD5u, 0x80u, 0xA8u, 0x3Fu, 0xD3u, 0x00u, 0x40u,
        0x8Du, 0x14u, 0x20u, 0x00u, 0xA1u, 0xC8u, 0xDAu, 0x12u,
        0x81u, 0x8Fu, 0xD9u, 0xF1u, 0xDBu, 0x40u, 0xA4u, 0xA6u,
        0xAFu
    };
    uint8_t i;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    gpio.GPIO_Pin = OLED_SCL_PIN | OLED_SDA_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);
    OLEDI2C_Scl(1u);
    OLEDI2C_Sda(1u);
    s_initialized = 1u;
    s_ready = 0u;
    s_address = 0u;
    delay_ms(20u);

    if (OLEDI2C_AddressProbe(OLED_I2C_ADDRESS_1) != 0u)
    {
        s_address = OLED_I2C_ADDRESS_1;
    }
    else if (OLEDI2C_AddressProbe(OLED_I2C_ADDRESS_2) != 0u)
    {
        s_address = OLED_I2C_ADDRESS_2;
    }
    else
    {
        return OLED_I2C_NOT_FOUND;
    }
    s_ready = 1u;

    for (i = 0u; i < sizeof(init_commands); ++i)
    {
        if (OLEDI2C_Command(init_commands[i]) != OLED_I2C_OK)
        {
            s_ready = 0u;
            return OLED_I2C_NOT_FOUND;
        }
    }
    return OLEDI2C_Clear();
}

/**
 * @brief 查询 OLED 是否已经就绪。
 */
uint8_t OLEDI2C_IsReady(void)
{
    return s_ready;
}

/**
 * @brief 获取 OLED 的 7 位 I2C 地址。
 */
uint8_t OLEDI2C_GetAddress(void)
{
    return s_address;
}

/**
 * @brief 清空 OLED 的 8 个显示页。
 */
OLEDI2CStatus OLEDI2C_Clear(void)
{
    uint8_t page;
    uint8_t column;
    uint8_t zeros[16];
    OLEDI2CStatus status;
    for (column = 0u; column < sizeof(zeros); ++column)
    {
        zeros[column] = 0u;
    }
    for (page = 0u; page < 8u; ++page)
    {
        status = OLEDI2C_SetPosition(0u, page);
        if (status != OLED_I2C_OK)
        {
            return status;
        }
        for (column = 0u; column < 8u; ++column)
        {
            status = OLEDI2C_Write(0x40u, zeros, sizeof(zeros));
            if (status != OLED_I2C_OK)
            {
                return status;
            }
        }
    }
    return OLED_I2C_OK;
}

/**
 * @brief 显示一个 6 列宽的 ASCII 字符。
 */
OLEDI2CStatus OLEDI2C_ShowChar(uint8_t x, uint8_t page, char character)
{
    uint8_t data[6];
    uint8_t column;
    OLEDI2CStatus status;
    if (x > 122u || page > 7u)
    {
        return OLED_I2C_ARGUMENT;
    }
    status = OLEDI2C_SetPosition(x, page);
    if (status != OLED_I2C_OK)
    {
        return status;
    }
    for (column = 0u; column < 5u; ++column)
    {
        data[column] = OLEDI2C_Glyph(character, column);
    }
    data[5] = 0u;
    return OLEDI2C_Write(0x40u, data, sizeof(data));
}

/**
 * @brief 显示一行 ASCII 字符串。
 */
OLEDI2CStatus OLEDI2C_ShowString(uint8_t x, uint8_t page, const char *text)
{
    OLEDI2CStatus status;
    if (text == 0 || x > 122u || page > 7u)
    {
        return OLED_I2C_ARGUMENT;
    }
    while (*text != '\0' && x <= 122u)
    {
        status = OLEDI2C_ShowChar(x, page, *text++);
        if (status != OLED_I2C_OK)
        {
            return status;
        }
        x = (uint8_t)(x + 6u);
    }
    return OLED_I2C_OK;
}
