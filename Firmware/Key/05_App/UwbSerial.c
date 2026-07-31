#include "UwbSerial.h"

#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"

#define UWB_SERIAL_BUFFER_SIZE 512u
#define UWB_SERIAL_TX_TIMEOUT  100000u

static volatile uint8_t s_buffer[UWB_SERIAL_BUFFER_SIZE];
static volatile uint16_t s_head;
static volatile uint16_t s_tail;
static volatile uint8_t s_overflow;

/** @brief 将中断收到的字节放入环形缓冲区。 */
static void UwbSerial_Push(uint8_t value)
{
    uint16_t next_head = (uint16_t)((s_head + 1u) % UWB_SERIAL_BUFFER_SIZE);
    if (next_head == s_tail)
    {
        s_overflow = 1u;
        return;
    }
    s_buffer[s_head] = value;
    s_head = next_head;
}

/** @brief 初始化 USART1 PA9/PA10 和接收中断。 */
void UwbSerial_Init(uint32_t baud_rate)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;
    NVIC_InitTypeDef nvic;

    s_head = 0u;
    s_tail = 0u;
    s_overflow = 0u;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO |
                           RCC_APB2Periph_USART1, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_USART1, DISABLE);
    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    USART_DeInit(USART1);
    usart.USART_BaudRate = baud_rate;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &usart);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART1, ENABLE);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    nvic.NVIC_IRQChannel = USART1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1u;
    nvic.NVIC_IRQChannelSubPriority = 0u;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

/** @brief Send one zero-terminated ASCII command over USART1. */
uint8_t UwbSerial_WriteText(const char *text)
{
    uint32_t timeout;

    if (text == 0) return 0u;
    while (*text != '\0')
    {
        timeout = UWB_SERIAL_TX_TIMEOUT;
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
        {
            if (timeout-- == 0u) return 0u;
        }
        USART_SendData(USART1, (uint8_t)*text++);
    }

    timeout = UWB_SERIAL_TX_TIMEOUT;
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
    {
        if (timeout-- == 0u) return 0u;
    }
    return 1u;
}

/** @brief 从环形缓冲区读取一个字节。 */
uint8_t UwbSerial_ReadByte(uint8_t *value)
{
    if (value == 0 || s_head == s_tail) return 0u;
    *value = s_buffer[s_tail];
    s_tail = (uint16_t)((s_tail + 1u) % UWB_SERIAL_BUFFER_SIZE);
    return 1u;
}

/** @brief 读取并清除 UART 溢出状态。 */
uint8_t UwbSerial_TakeOverflow(void)
{
    uint8_t overflow = s_overflow;
    s_overflow = 0u;
    return overflow;
}

/** @brief USART1 中断仅收取字节，解析留给主循环。 */
void UwbSerial_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        UwbSerial_Push((uint8_t)USART_ReceiveData(USART1));
    }
}
