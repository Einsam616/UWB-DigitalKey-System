#include "DipSwitch.h"

#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

/** @brief Initialize active-low SW1..SW4 inputs with internal pull-ups. */
void DipSwitch_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_GPIOB, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_Init(GPIOB, &gpio);
}

/** @brief Pack SW1..SW4 into the displayed MSB-first four-bit identity. */
uint8_t DipSwitch_ReadValue(void)
{
    uint8_t value = 0u;

    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6) == Bit_RESET) value |= 0x08u;
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_7) == Bit_RESET) value |= 0x04u;
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_9) == Bit_RESET) value |= 0x02u;
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_10) == Bit_RESET) value |= 0x01u;
    return value;
}
