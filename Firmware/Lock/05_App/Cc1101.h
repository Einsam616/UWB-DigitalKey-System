#ifndef CC1101_H
#define CC1101_H

#include <stdint.h>

typedef enum
{
    CC1101_TX_MODE = 0,
    CC1101_RX_MODE = 1
} Cc1101TrMode;

/**
 * @brief 初始化重映射 SPI1、CC1101 控制脚和 CC1101 寄存器。
 * @return 芯片型号正确返回 0，否则返回负值；即使未插模块，SPI 和寄存器访问也会完成。
 */
int Cc1101_Init(void);

/**
 * @brief 读取 CC1101 的状态寄存器。
 * @param address 状态寄存器地址，使用 Cc1101Regs.h 中的定义。
 * @return 读取到的状态字节。
 */
uint8_t Cc1101_ReadStatus(uint8_t address);

/**
 * @brief 读取 CC1101 配置寄存器。
 * @param address 配置寄存器地址。
 * @return 读取到的寄存器值。
 */
uint8_t Cc1101_ReadRegister(uint8_t address);

/**
 * @brief 设置 CC1101 为发送或接收状态。
 * @param mode 发送或接收模式。
 * @return 无。
 */
void Cc1101_SetMode(Cc1101TrMode mode);

/**
 * @brief 发送一个可变长度、带硬件 CRC 的数据包。
 * @param data 有效载荷指针。
 * @param length 有效载荷长度，范围 1 到 60 字节。
 * @return 成功发完并检测到 GDO0 结束沿返回 0，否则返回负值。
 * @note 2 kbps 空口速率下最长等待约 150 ms，适合本次最小点对点测试。
 */
int Cc1101_SendPacket(const uint8_t *data, uint8_t length);

/**
 * @brief 从 RX FIFO 读取一个 CRC 正确的数据包。
 * @param data 输出缓冲区。
 * @param capacity 输出缓冲区容量。
 * @param length 输出实际载荷长度。
 * @return 读到合法包返回 1，无包返回 0，FIFO 或参数错误返回负值。
 * @note 本函数不等待无线事件，适合在主循环中轮询。
 */
int Cc1101_ReceivePacket(uint8_t *data, uint8_t capacity, uint8_t *length);

/**
 * @brief 等待 GDO0 完成一次低、高、低的数据包脉冲。
 * @param timeout_us 超时时间，0 表示使用默认 150 ms。
 * @return 检测到完整脉冲返回 1，超时返回 0。
 */
uint8_t Cc1101_WaitGdo0(uint32_t timeout_us);

/**
 * @brief 获取上次初始化读到的芯片型号和版本。
 * @param part_number 芯片型号输出指针，可为空。
 * @param version 芯片版本输出指针，可为空。
 * @return 无。
 */
void Cc1101_GetIdentity(uint8_t *part_number, uint8_t *version);

#endif
