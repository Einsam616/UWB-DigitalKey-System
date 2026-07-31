#include "LockIo.h"

#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"

#define LOCK_BUZZER_PORT GPIOB
#define LOCK_BUZZER_PIN  GPIO_Pin_8
#define LOCK_BUZZER_PERIOD   499u
#define LOCK_BUZZER_PULSE    250u
#define LOCK_BUZZER_IDLE     (LOCK_BUZZER_PERIOD + 1u)
#define LOCK_LED_PORT        GPIOA
#define LOCK_LED_ALL_PINS    (GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | \
                              GPIO_Pin_3 | GPIO_Pin_4)
#define LOCK_LED_ACTIVE_LOW  1

static const uint16_t s_led_pins[LOCK_LED_COUNT] = {
    GPIO_Pin_0, GPIO_Pin_1, GPIO_Pin_2, GPIO_Pin_3, GPIO_Pin_4
};

static uint32_t s_buzzer_deadline;
static uint8_t s_buzzer_active;
static uint8_t s_led_mask;

/** @brief Initialize PA0-PA4 LEDs and PB8 active-low buzzer output. */
void LockIo_Init(void)
{
    GPIO_InitTypeDef gpio;
    TIM_TimeBaseInitTypeDef timer;
    TIM_OCInitTypeDef pwm;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

    /* Preload the inactive LED level before changing the pins to outputs. */
#if (LOCK_LED_ACTIVE_LOW == 1)
    GPIO_SetBits(LOCK_LED_PORT, LOCK_LED_ALL_PINS);
#else
    GPIO_ResetBits(LOCK_LED_PORT, LOCK_LED_ALL_PINS);
#endif
    gpio.GPIO_Pin = LOCK_LED_ALL_PINS;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(LOCK_LED_PORT, &gpio);
    s_led_mask = 0u;

    gpio.GPIO_Pin = LOCK_BUZZER_PIN;
    /* The buzzer driver is active-low. Set the safe level before enabling AF. */
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LOCK_BUZZER_PORT, &gpio);
    GPIO_SetBits(LOCK_BUZZER_PORT, LOCK_BUZZER_PIN);

    timer.TIM_Period = LOCK_BUZZER_PERIOD;
    timer.TIM_Prescaler = 71u;
    timer.TIM_ClockDivision = TIM_CKD_DIV1;
    timer.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM4, &timer);

    pwm.TIM_OCMode = TIM_OCMode_PWM1;
    pwm.TIM_OutputState = TIM_OutputState_Enable;
    pwm.TIM_Pulse = LOCK_BUZZER_IDLE;
    pwm.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC3Init(TIM4, &pwm);
    TIM_OC3PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM4, ENABLE);
    TIM_Cmd(TIM4, ENABLE);

    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(LOCK_BUZZER_PORT, &gpio);

    s_buzzer_deadline = 0u;
    s_buzzer_active = 0u;
    LockIo_SetBuzzer(0u);
}

/** @brief Set all PA0-PA4 LEDs while honoring the board polarity. */
void LockIo_SetLedMask(uint8_t mask)
{
    uint16_t enabled_pins = 0u;
    uint16_t disabled_pins;
    uint8_t index;

    mask &= (uint8_t)((1u << (uint8_t)LOCK_LED_COUNT) - 1u);
    if (mask == s_led_mask) return;

    for (index = 0u; index < (uint8_t)LOCK_LED_COUNT; ++index)
    {
        if ((mask & LOCK_LED_BIT(index)) != 0u)
        {
            enabled_pins |= s_led_pins[index];
        }
    }
    disabled_pins = (uint16_t)(LOCK_LED_ALL_PINS & (uint16_t)~enabled_pins);

#if (LOCK_LED_ACTIVE_LOW == 1)
    GPIO_SetBits(LOCK_LED_PORT, disabled_pins);
    GPIO_ResetBits(LOCK_LED_PORT, enabled_pins);
#else
    GPIO_ResetBits(LOCK_LED_PORT, disabled_pins);
    GPIO_SetBits(LOCK_LED_PORT, enabled_pins);
#endif
    s_led_mask = mask;
}

/** @brief Set one status LED without disturbing the other four. */
void LockIo_SetLed(LockIoLed led, uint8_t enabled)
{
    uint8_t bit;

    if ((uint8_t)led >= (uint8_t)LOCK_LED_COUNT) return;
    bit = LOCK_LED_BIT(led);
    if (enabled != 0u)
    {
        LockIo_SetLedMask((uint8_t)(s_led_mask | bit));
    }
    else
    {
        LockIo_SetLedMask((uint8_t)(s_led_mask & (uint8_t)~bit));
    }
}

/** @brief Enable or disable the PB8 2 kHz PWM waveform. */
void LockIo_SetBuzzer(uint8_t enabled)
{
    /* PWM1 is high while CNT < CCR. Keep PB8 high when idle. */
    TIM_SetCompare3(TIM4, (enabled != 0u) ? LOCK_BUZZER_PULSE : LOCK_BUZZER_IDLE);
}

/** @brief Start or extend a non-blocking buzzer pulse. */
void LockIo_BeepUntil(uint32_t tick_ms, uint16_t duration_ms)
{
    s_buzzer_deadline = tick_ms + (uint32_t)duration_ms;
    s_buzzer_active = 1u;
    LockIo_SetBuzzer(1u);
}

/** @brief Turn off the buzzer after its deadline. */
void LockIo_Service(uint32_t tick_ms)
{
    if (s_buzzer_active != 0u && (int32_t)(tick_ms - s_buzzer_deadline) >= 0)
    {
        s_buzzer_active = 0u;
        LockIo_SetBuzzer(0u);
    }
}
