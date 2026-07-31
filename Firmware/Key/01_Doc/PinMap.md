# 数字钥匙 Key 端接线

目标芯片：STM32F103C8T6，Keil 工程：`STM32F103_Base32.uvprojx`。

| 模块 | 信号 | MCU 引脚 | 方向/模式 | 外设 | 电气说明 |
|---|---|---|---|---|---|
| UWB Anchor | Anchor RXD | PA9 | MCU 输出 | USART1_TX | 交叉连接：MCU PA9 -> Anchor RXD |
| UWB Anchor | Anchor TXD | PA10 | MCU 输入 | USART1_RX | 交叉连接：Anchor TXD -> MCU PA10 |
| UWB Anchor | GND | GND | 电源地 | - | 必须共地 |
| UWB Anchor | VCC | 按模块手册 | 电源 | - | 先确认模块电平，不直接假设 5 V |
| OLED | SCL | PA4 | 开漏输出 | 软件 I2C | 需要上拉到 3.3 V |
| OLED | SDA | PA6 | 开漏/输入 | 软件 I2C | 需要上拉到 3.3 V；PA5 已给 CC1101 GDO0 |
| OLED | VCC/GND | 3.3 V/GND | 电源 | - | OLED 与 STM32 共地 |
| CC1101 | GDO0 | PA5 | 下拉输入 | GPIO | 数据包状态脉冲 |
| CC1101 | SPI1 SCK | PB3 | 复用推挽输出 | SPI1 全重映射 | 关闭 JTAG，保留 SWD |
| CC1101 | SPI1 MISO/GDO1 | PB4 | 上拉输入 | SPI1 全重映射 | 从机数据输出 |
| CC1101 | SPI1 MOSI | PB5 | 复用推挽输出 | SPI1 全重映射 | 主机数据输出 |
| CC1101 | CSN | PB11 | 推挽输出 | GPIO | Key 端独立片选，低有效 |
| 拨码开关 | SW1 | PB12 | 内部上拉输入 | GPIO | ON 接 GND，显示 ID 第1位为1 |
| 拨码开关 | SW2 | PB13 | 内部上拉输入 | GPIO | ON 接 GND，显示 ID 第2位为1 |
| 拨码开关 | SW3 | PB14 | 内部上拉输入 | GPIO | ON 接 GND，显示 ID 第3位为1 |
| 拨码开关 | SW4 | PB15 | 内部上拉输入 | GPIO | ON 接 GND，显示 ID 第4位为1 |

注意：Key 端 CC1101 的 SPI 与 GDO0 与 Lock 端一致，但 CSN 改为 PB11，避开 PB12~PB15 拨码。CSN 是 MCU 本地片选，两端引脚不同不影响 CC1101 空中通信。
