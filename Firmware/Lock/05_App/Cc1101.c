#include "Cc1101.h"

#include "Cc1101Regs.h"
#include "delay.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_spi.h"

#define CC1101_CS_PORT       GPIOB
#define CC1101_CS_PIN        GPIO_Pin_12
#define CC1101_GDO0_PORT     GPIOA
#define CC1101_GDO0_PIN      GPIO_Pin_5
#define CC1101_MISO_PORT     GPIOB
#define CC1101_MISO_PIN      GPIO_Pin_4
#define CC1101_SPI           SPI1
#define CC1101_MAX_PAYLOAD   60u
#define CC1101_DEFAULT_WAIT  150000u
#define CC1101_WRITE_BURST   0x40u
#define CC1101_READ_SINGLE   0x80u
#define CC1101_READ_BURST    0xC0u
#define CC1101_RX_COUNT_MASK 0x7Fu
#define CC1101_CRC_OK        0x80u

static const uint8_t s_pa_table[8] = {0xC0u, 0xC8u, 0x84u, 0x60u, 0x68u, 0x34u, 0x1Du, 0x0Eu};
static const uint8_t s_init_data[][2] =
{
    {CC1101_IOCFG0, 0x06u},
    {CC1101_FIFOTHR, 0x47u},
    {CC1101_PKTCTRL0, 0x05u},
    {CC1101_PKTCTRL1, 0x04u},
    {CC1101_PKTLEN, 0x3Du},
    {CC1101_CHANNR, 0x01u},
    {CC1101_FSCTRL1, 0x06u},
    {CC1101_FREQ2, 0x0Fu},
    {CC1101_FREQ1, 0x62u},
    {CC1101_FREQ0, 0x76u},
    {CC1101_MDMCFG4, 0xF6u},
    {CC1101_MDMCFG3, 0x43u},
    {CC1101_MDMCFG2, 0x13u},
    {CC1101_MDMCFG1, 0x72u},
    {CC1101_DEVIATN, 0x15u},
    {CC1101_MCSM2, 0x07u},
    {CC1101_MCSM1, 0x3Fu},
    {CC1101_MCSM0, 0x18u},
    {CC1101_FOCCFG, 0x16u},
    {CC1101_FSCAL3, 0xE9u},
    {CC1101_FSCAL2, 0x2Au},
    {CC1101_FSCAL1, 0x00u},
    {CC1101_FSCAL0, 0x1Fu},
    {CC1101_TEST2, 0x81u},
    {CC1101_TEST1, 0x35u}
};

static uint8_t s_part_number;
static uint8_t s_version;

/** @brief 拉低片选并等待 CC1101 的 MISO 就绪。 */
static uint8_t Cc1101_Select(void)
{
    uint32_t timeout = 100000u;
    GPIO_ResetBits(CC1101_CS_PORT, CC1101_CS_PIN);
    while (GPIO_ReadInputDataBit(CC1101_MISO_PORT, CC1101_MISO_PIN) != Bit_RESET)
    {
        if (timeout-- == 0u)
        {
            GPIO_SetBits(CC1101_CS_PORT, CC1101_CS_PIN);
            return 0u;
        }
    }
    return 1u;
}

/** @brief 释放 CC1101 片选。 */
static void Cc1101_Deselect(void)
{
    GPIO_SetBits(CC1101_CS_PORT, CC1101_CS_PIN);
}

/** @brief 通过重映射到 PB3/PB4/PB5 的 SPI1 交换一个字节。 */
static uint8_t Cc1101_SpiExchange(uint8_t value)
{
    while (SPI_I2S_GetFlagStatus(CC1101_SPI, SPI_I2S_FLAG_TXE) == RESET) {}
    SPI_I2S_SendData(CC1101_SPI, value);
    while (SPI_I2S_GetFlagStatus(CC1101_SPI, SPI_I2S_FLAG_RXNE) == RESET) {}
    return (uint8_t)SPI_I2S_ReceiveData(CC1101_SPI);
}

/** @brief 写入 CC1101 命令字节。 */
static void Cc1101_Strobe(uint8_t command)
{
    if (Cc1101_Select() != 0u)
    {
        (void)Cc1101_SpiExchange(command);
        Cc1101_Deselect();
    }
}

/** @brief 写入一个配置寄存器。 */
static void Cc1101_WriteRegister(uint8_t address, uint8_t value)
{
    if (Cc1101_Select() != 0u)
    {
        (void)Cc1101_SpiExchange(address);
        (void)Cc1101_SpiExchange(value);
        Cc1101_Deselect();
    }
}

/** @brief 连续写入 FIFO 或 PA 表。 */
static void Cc1101_WriteBurst(uint8_t address, const uint8_t *data, uint8_t length)
{
    uint8_t index;
    if (Cc1101_Select() == 0u) return;
    (void)Cc1101_SpiExchange((uint8_t)(address | CC1101_WRITE_BURST));
    for (index = 0u; index < length; ++index) (void)Cc1101_SpiExchange(data[index]);
    Cc1101_Deselect();
}

/** @brief 连续读取 FIFO 或状态数据。 */
static void Cc1101_ReadBurst(uint8_t address, uint8_t *data, uint8_t length)
{
    uint8_t index;
    if (Cc1101_Select() == 0u) return;
    (void)Cc1101_SpiExchange((uint8_t)(address | CC1101_READ_BURST));
    for (index = 0u; index < length; ++index) data[index] = Cc1101_SpiExchange(0xFFu);
    Cc1101_Deselect();
}

/** @brief 初始化重映射 SPI1 和 CC1101 控制引脚。 */
static void Cc1101_InitGpio(void)
{
    GPIO_InitTypeDef gpio;
    SPI_InitTypeDef spi;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_GPIOB | RCC_APB2Periph_SPI1, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SPI1, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_5;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);
    gpio.GPIO_Pin = CC1101_MISO_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &gpio);
    gpio.GPIO_Pin = CC1101_CS_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &gpio);
    GPIO_SetBits(GPIOB, CC1101_CS_PIN);
    gpio.GPIO_Pin = CC1101_GDO0_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(CC1101_GDO0_PORT, &gpio);

    SPI_I2S_DeInit(CC1101_SPI);
    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Master;
    spi.SPI_DataSize = SPI_DataSize_8b;
    spi.SPI_CPOL = SPI_CPOL_Low;
    spi.SPI_CPHA = SPI_CPHA_1Edge;
    spi.SPI_NSS = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;
    spi.SPI_FirstBit = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial = 7u;
    SPI_Init(CC1101_SPI, &spi);
    SPI_Cmd(CC1101_SPI, ENABLE);
}

/** @brief 读取 CC1101 配置寄存器。 */
uint8_t Cc1101_ReadRegister(uint8_t address)
{
    uint8_t value = 0xFFu;
    if (Cc1101_Select() != 0u)
    {
        (void)Cc1101_SpiExchange((uint8_t)(address | CC1101_READ_SINGLE));
        value = Cc1101_SpiExchange(0xFFu);
        Cc1101_Deselect();
    }
    return value;
}

/** @brief 读取 CC1101 状态寄存器。 */
uint8_t Cc1101_ReadStatus(uint8_t address)
{
    uint8_t value = 0xFFu;
    if (Cc1101_Select() != 0u)
    {
        (void)Cc1101_SpiExchange((uint8_t)(address | CC1101_READ_BURST));
        value = Cc1101_SpiExchange(0xFFu);
        Cc1101_Deselect();
    }
    return value;
}

/** @brief 复位 CC1101。 */
static void Cc1101_Reset(void)
{
    Cc1101_Deselect();
    delay_us(1u);
    if (Cc1101_Select() != 0u) Cc1101_Deselect();
    delay_us(41u);
    Cc1101_Strobe(CC1101_SRES);
    delay_ms(1u);
}

/** @brief 清空发送 FIFO。 */
static void Cc1101_FlushTx(void)
{
    Cc1101_Strobe(CC1101_SIDLE);
    Cc1101_Strobe(CC1101_SFTX);
}

/** @brief 清空接收 FIFO。 */
static void Cc1101_FlushRx(void)
{
    Cc1101_Strobe(CC1101_SIDLE);
    Cc1101_Strobe(CC1101_SFRX);
}

/**
 * @brief 等待 GDO0 完成低、高、低的完整数据包脉冲。
 */
uint8_t Cc1101_WaitGdo0(uint32_t timeout_us)
{
    uint32_t elapsed = 0u;
    if (timeout_us == 0u) timeout_us = CC1101_DEFAULT_WAIT;
    while (GPIO_ReadInputDataBit(CC1101_GDO0_PORT, CC1101_GDO0_PIN) != Bit_RESET)
    {
        if (elapsed >= timeout_us) return 0u;
        delay_us(1u);
        elapsed++;
    }
    while (GPIO_ReadInputDataBit(CC1101_GDO0_PORT, CC1101_GDO0_PIN) == Bit_RESET)
    {
        if (elapsed >= timeout_us) return 0u;
        delay_us(1u);
        elapsed++;
    }
    while (GPIO_ReadInputDataBit(CC1101_GDO0_PORT, CC1101_GDO0_PIN) != Bit_RESET)
    {
        if (elapsed >= timeout_us) return 0u;
        delay_us(1u);
        elapsed++;
    }
    return 1u;
}

/** @brief 设置 CC1101 的发送或接收状态。 */
void Cc1101_SetMode(Cc1101TrMode mode)
{
    Cc1101_WriteRegister(CC1101_IOCFG0, 0x06u);
    Cc1101_Strobe((mode == CC1101_TX_MODE) ? CC1101_STX : CC1101_SRX);
}

/** @brief 发送一个数据包。 */
int Cc1101_SendPacket(const uint8_t *data, uint8_t length)
{
    if (data == 0 || length == 0u || length > CC1101_MAX_PAYLOAD) return -1;
    Cc1101_FlushTx();
    Cc1101_WriteRegister(CC1101_TXFIFO, length);
    Cc1101_WriteBurst(CC1101_TXFIFO, data, length);
    Cc1101_SetMode(CC1101_TX_MODE);
    if (Cc1101_WaitGdo0(CC1101_DEFAULT_WAIT) == 0u)
    {
        Cc1101_FlushTx();
        return -2;
    }
    Cc1101_FlushTx();
    return 0;
}

/** @brief 从 RX FIFO 读取一个合法数据包。 */
int Cc1101_ReceivePacket(uint8_t *data, uint8_t capacity, uint8_t *length)
{
    uint8_t raw_count;
    uint8_t count;
    uint8_t packet_length;
    uint8_t status[2];
    if (data == 0 || length == 0 || capacity == 0u) return -1;
    *length = 0u;
    if (GPIO_ReadInputDataBit(CC1101_GDO0_PORT, CC1101_GDO0_PIN) != Bit_RESET) return 0;
    raw_count = Cc1101_ReadStatus(CC1101_RXBYTES);
    if ((raw_count & 0x80u) != 0u)
    {
        Cc1101_FlushRx();
        Cc1101_SetMode(CC1101_RX_MODE);
        return -4;
    }
    count = (uint8_t)(raw_count & CC1101_RX_COUNT_MASK);
    if (count == 0u) return 0;
    packet_length = Cc1101_ReadRegister(CC1101_RXFIFO);
    if (packet_length == 0u || packet_length > capacity ||
        packet_length > CC1101_MAX_PAYLOAD || count < (uint8_t)(packet_length + 3u))
    {
        Cc1101_FlushRx();
        Cc1101_SetMode(CC1101_RX_MODE);
        return -2;
    }
    Cc1101_ReadBurst(CC1101_RXFIFO, data, packet_length);
    Cc1101_ReadBurst(CC1101_RXFIFO, status, 2u);
    Cc1101_FlushRx();
    Cc1101_SetMode(CC1101_RX_MODE);
    if ((status[1] & CC1101_CRC_OK) == 0u) return -3;
    *length = packet_length;
    return 1;
}

/** @brief 初始化 CC1101 寄存器并读取芯片身份。 */
int Cc1101_Init(void)
{
    uint8_t index;
    Cc1101_InitGpio();
    Cc1101_Reset();
    for (index = 0u; index < (uint8_t)(sizeof(s_init_data) / sizeof(s_init_data[0])); ++index)
    {
        Cc1101_WriteRegister(s_init_data[index][0], s_init_data[index][1]);
    }
    Cc1101_WriteRegister(CC1101_ADDR, 0x05u);
    Cc1101_WriteRegister(CC1101_SYNC1, 0x87u);
    Cc1101_WriteRegister(CC1101_SYNC0, 0x99u);
    Cc1101_WriteBurst(CC1101_PATABLE, s_pa_table, sizeof(s_pa_table));
    s_part_number = Cc1101_ReadStatus(CC1101_PARTNUM);
    s_version = Cc1101_ReadStatus(CC1101_VERSION);
    Cc1101_SetMode(CC1101_RX_MODE);
    return (s_part_number == 0x00u) ? 0 : -1;
}

/** @brief 读取最近一次初始化的芯片型号和版本。 */
void Cc1101_GetIdentity(uint8_t *part_number, uint8_t *version)
{
    if (part_number != 0) *part_number = s_part_number;
    if (version != 0) *version = s_version;
}
