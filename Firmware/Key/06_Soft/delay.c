#include "delay.h"
#include "stm32f10x.h"

void delay_us(uint32_t us)
{
    SysTick->LOAD = (SystemCoreClock / 1000000u) * us;
    SysTick->VAL = 0x00u;
    SysTick->CTRL = 0x00000005u;
    while ((SysTick->CTRL & 0x00010000u) == 0u) {
    }
    SysTick->CTRL = 0x00000004u;
}

void delay_ms(uint32_t ms)
{
    while (ms-- > 0u) {
        delay_us(1000u);
    }
}

void delay_s(uint32_t s)
{
    while (s-- > 0u) {
        delay_ms(1000u);
    }
}
