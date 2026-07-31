#include "DipSwitch.h"

#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

static const uint16_t s_pins[4] =
{
    GPIO_Pin_12, GPIO_Pin_13, GPIO_Pin_14, GPIO_Pin_15
};

/** @brief Initialize PB12..PB15 as active-low inputs with internal pull-ups. */
void DipSwitch_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    gpio.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &gpio);
}

/** @brief Convert grounded ON states on PB12..PB15 to four logical ones. */
void DipSwitch_ReadBits(uint8_t bits[4])
{
    uint8_t index;

    if (bits == 0) return;
    for (index = 0u; index < 4u; ++index)
    {
        bits[index] =
            (GPIO_ReadInputDataBit(GPIOB, s_pins[index]) == Bit_RESET) ? 1u : 0u;
    }
}

/** @brief Pack the four switches in their displayed MSB-first order. */
uint8_t DipSwitch_ReadValue(void)
{
    uint8_t bits[4];

    DipSwitch_ReadBits(bits);
    return (uint8_t)((bits[0] << 3u) | (bits[1] << 2u) |
                     (bits[2] << 1u) | bits[3]);
}
