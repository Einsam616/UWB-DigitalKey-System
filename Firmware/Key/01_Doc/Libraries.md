# 数字钥匙 Key 端模块清单

| 模块 | 来源 | 作用 | 工程文件 | 适配说明 |
|---|---|---|---|---|
| STM32F10x Standard Peripheral Library | 基础框架 `04_Driver` | GPIO、RCC、SPI、USART | `stm32f10x_*.c/.h` | 使用 STM32F103 标准库 |
| OLEDI2C | `E:\Work\Competition\电赛\hc05测试\02_Receiver\05_App` | SSD1306 软件 I2C ASCII 显示 | `OLEDI2C.c/.h` | PA4=SCL、PA6=SDA，避让 PA5 CC1101 GDO0 |
| CC1101 | `E:\Work\Competition\电赛\E06-STM8资料 - 141016\3  相关范例程序\CC1101\BSP` | CC1101 寄存器、FIFO、GDO0 收发 | `Cc1101.c/.h`, `Cc1101Regs.h` | 重映射 SPI1（PB3/PB4/PB5），GDO0=PA5，Key CSN=PB12 |
| UwbSerial | 本工程 | PA9 发送绑定命令，PA10 接收 Anchor `mc` 帧 | `UwbSerial.c/.h` | 256000, 8N1, 固定 USART1 不重映射，RX 中断环形缓冲 |
| UwbParser | 本工程 | 解析低功耗 V3 `mc` 的 RANGE0 和 Tag ID | `UwbParser.c/.h` | 扫描 `aXXXX:YYYY`，不写死 ID 与字段位置 |
| PositionSolver | 本工程 | 用左右两路距离解算圆心距离和方位角 | `PositionSolver.c/.h` | 基线 500 mm，4E21 为左、4E22 为右，右侧角度为正 |

Key 身份不是外设库：`05_App/App.c` 中的 `APP_KEY_ID=0x0D` 固定表示 `1101`，随每个正式 CC1101 帧发送。第二把钥匙应烧录另一个 4 位 ID，而不是在比赛运行时临时改动身份。
