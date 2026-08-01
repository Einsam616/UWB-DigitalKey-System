# Lock 端引脚表

目标芯片：STM32F103C8T6，逻辑电平 3.3 V。

| 模块 | 信号 | STM32 引脚 | 工作方式 | 外设/复用 | 电气说明 |
|---|---|---|---|---|---|
| 指示灯 | D1 | PA0 | 推挽输出 | GPIO | 低电平点亮；闭锁或身份错误 |
| 指示灯 | D2 | PA1 | 推挽输出 | GPIO | 低电平点亮；开锁 |
| 指示灯 | D3 | PA2 | 推挽输出 | GPIO | 低电平点亮；检测到钥匙 |
| 指示灯 | D4 | PA3 | 推挽输出 | GPIO | 低电平点亮；迎宾区 |
| 指示灯 | D5 | PA4 | 推挽输出 | GPIO | 低电平点亮；身份验证通过 |
| CC1101 | GDO0 | PA5 | 下拉输入 | GPIO | 接收数据同步/状态 |
| Lock 端拨码 | 第 1 位 | PA7 | 上拉输入 | GPIO | 低电平有效；拨到 ON 记为 1；ID 第 3 位 |
| Lock 端拨码 | 第 2 位 | PB1 | 上拉输入 | GPIO | 低电平有效；拨到 ON 记为 1；ID 第 2 位 |
| 彩屏 | CS1 | PA8 | 推挽输出 | GPIO | 低电平选中 |
| 调试串口 | USART1_TX/RX | PA9/PA10 | 复用输出/上拉输入 | USART1 | 115200，8N1 |
| 彩屏 | RES | PA11 | 推挽输出 | GPIO | 低电平复位 |
| 彩屏 | DC | PA12 | 推挽输出 | GPIO | 命令/数据选择 |
| 下载调试 | SWDIO/SWCLK | PA13/PA14 | 调试专用 | SWD | 保留连接，用于烧录和调试 |
| 彩屏 | CS2 | PA15 | 推挽输出 | GPIO | 保持高电平；关闭 JTAG 后可用 |
| 彩屏 | BLK | PB0 | 推挽输出 | GPIO/PWM | 背光控制 |
| 启动配置 | BOOT1 | PB2 | 板载下拉 | GPIO | 保持板上默认下拉 |
| CC1101 | SPI1_SCK | PB3 | 复用推挽输出 | SPI1 重映射 | 关闭 JTAG，保留 SWD |
| CC1101 | SPI1_MISO/GDO1 | PB4 | 上拉输入 | SPI1 重映射 | 与模块状态线复用 |
| CC1101 | SPI1_MOSI | PB5 | 复用推挽输出 | SPI1 重映射 | 模块数据输入 |
| TM1637 键盘/数码管 | CLK | PB6 | 开漏输出 | GPIO | 键盘时钟和状态显示 |
| TM1637 键盘/数码管 | IO | PB7 | 开漏输入/输出 | GPIO | 键盘数据和 P/F 显示 |
| 蜂鸣器 | PWM | PB8 | 复用推挽输出 | TIM4_CH3 | 2 kHz，工作时约 50% 占空比 |
| 确认按键 | APPLY | PB9 | 上拉输入 | GPIO | 低电平有效；消抖后应用待设定 ID |
| Lock 端拨码 | 第 3 位 | PB10 | 上拉输入 | GPIO | 低电平有效；拨到 ON 记为 1；ID 第 1 位 |
| Lock 端拨码 | 第 4 位 | PB11 | 上拉输入 | GPIO | 低电平有效；拨到 ON 记为 1；ID 第 0 位 |
| CC1101 | CSN | PB12 | 推挽输出 | GPIO | 低电平选中 |
| 彩屏 | SPI2_SCK/CLK | PB13 | 推挽输出 | 软件 SPI | 屏幕时钟 |
| 彩屏 | SPI2_MISO | PB14 | 预留输入 | GPIO | 当前页面不读取，仅保留接线 |
| 彩屏 | SPI2_MOSI | PB15 | 推挽输出 | 软件 SPI | 屏幕数据 |
| 调试探针 | 收发活动指示 | PC13 | 推挽输出 | GPIO | 低电平有效，收到有效无线包时拉低约 30 ms；当前替代 PEN |

## 拨码位顺序

Lock 端四位拨码从高位到低位读取：

```text
第 1 位 PA7  -> ID[3]
第 2 位 PB1  -> ID[2]
第 3 位 PB10 -> ID[1]
第 4 位 PB11 -> ID[0]
```

拨码输入使用内部上拉，拨码另一端接地：

```text
OFF = 0（输入为高电平）
ON  = 1（输入被接地为低电平）
```

## SPI1 重映射

CC1101 使用 SPI1 完全重映射。驱动初始化时执行：

```c
RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
GPIO_PinRemapConfig(GPIO_Remap_SPI1, ENABLE);
```

这样会关闭 JTAG，但 PA13/PA14 的 SWD 下载调试仍然可用。

## 彩屏接口说明

彩屏沿用现有库的软件 SPI 时序，接线为 `PB13=CLK`、`PB15=MOSI`、`PA8=CS1`、`PA15=CS2`、`PA11=RES`、`PA12=DC`、`PB0=BLK`。当前页面只写屏，不读取 `PB14=MISO`。`CS2` 保持高电平，触摸功能暂不启用；后续恢复触摸时，PC13 改回 PEN 中断输入，并移除无线活动探针功能。
