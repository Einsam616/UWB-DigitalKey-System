#include "DipSwitch.h"

#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

#define DIP_SWITCH_DEBOUNCE_MS 5u
#define DIP_SWITCH_CONFIRM_PIN GPIO_Pin_9

static uint8_t s_raw_id;
static uint8_t s_id;
static uint8_t s_confirm_raw;
static uint8_t s_confirm_stable;
static uint32_t s_confirm_changed_ms;

static uint8_t DipSwitch_ReadRawValue(void)
{
    uint8_t value = 0u;

    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_7) == Bit_RESET) value |= 0x08u;
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == Bit_RESET) value |= 0x04u;
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_10) == Bit_RESET) value |= 0x02u;
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11) == Bit_RESET) value |= 0x01u;
    return value;
}

static uint8_t DipSwitch_ReadConfirm(void)
{
    return (GPIO_ReadInputDataBit(GPIOB, DIP_SWITCH_CONFIRM_PIN) == Bit_RESET) ?
           1u : 0u;
}

void DipSwitch_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_GPIOB, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_7;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_Init(GPIOB, &gpio);

    s_raw_id = DipSwitch_ReadRawValue();
    s_id = s_raw_id;
    s_confirm_raw = DipSwitch_ReadConfirm();
    s_confirm_stable = s_confirm_raw;
    s_confirm_changed_ms = 0u;
}

uint8_t DipSwitch_Service(uint32_t tick_ms)
{
    uint8_t raw_id = DipSwitch_ReadRawValue();
    uint8_t confirm_raw = DipSwitch_ReadConfirm();
    uint8_t events = DIP_SWITCH_EVENT_NONE;

    /* The four-position switch is sampled directly so its candidate ID
       appears as soon as the hardware level changes. */
    if (raw_id != s_id)
    {
        s_raw_id = raw_id;
        s_id = raw_id;
        events |= DIP_SWITCH_EVENT_CHANGED;
    }

    if (confirm_raw != s_confirm_raw)
    {
        s_confirm_raw = confirm_raw;
        s_confirm_changed_ms = tick_ms;
    }
    if (confirm_raw != s_confirm_stable &&
        (tick_ms - s_confirm_changed_ms) >= DIP_SWITCH_DEBOUNCE_MS)
    {
        s_confirm_stable = confirm_raw;
        if (s_confirm_stable != 0u)
        {
            events |= DIP_SWITCH_EVENT_APPLY;
        }
    }
    return events;
}

uint8_t DipSwitch_GetId(void)
{
    return s_id;
}
