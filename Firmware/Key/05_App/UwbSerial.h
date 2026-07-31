#ifndef UWB_SERIAL_H
#define UWB_SERIAL_H

#include <stdint.h>

/**
 * @brief 初始化 PA9/PA10 的 USART1，用于接收 UWB Anchor 的 ASCII mc 帧。
 * @param baud_rate UWB 串口波特率，当前使用 256000。
 * @return 无。
 */
void UwbSerial_Init(uint32_t baud_rate);

/**
 * @brief Send one zero-terminated ASCII command through USART1.
 * @param text Command text, including CR/LF when required by the device.
 * @return 1 after transmission completes, otherwise 0.
 */
uint8_t UwbSerial_WriteText(const char *text);

/**
 * @brief 从 USART1 环形缓冲区读取一个字节。
 * @param value 输出字节指针。
 * @return 读到字节返回 1，否则返回 0。
 */
uint8_t UwbSerial_ReadByte(uint8_t *value);

/**
 * @brief 返回并清除 UART 接收溢出标志。
 * @return 曾发生溢出返回 1，否则返回 0。
 */
uint8_t UwbSerial_TakeOverflow(void);

/**
 * @brief USART1 中断入口使用的最小字节接收处理。
 * @return 无。
 */
void UwbSerial_IRQHandler(void);

#endif
