# 数字钥匙 Key 端工程说明

本工程用于 STM32F103C8T6，PA9/PA10 以 `256000, 8N1` 自动绑定 `4E21/4E22` 并接收 UWB Anchor 的 `mc` 帧，解算距离和方位角后以 10 Hz 通过 CC1101 发送给 Lock 端。PA4/PA6 驱动 OLED，PA5 用于 CC1101 GDO0。

## 目录说明

- `01_Doc`: 文档说明
- `02_Core`: CMSIS Core 头文件
- `03_MCU`: 启动文件、芯片头文件、系统文件、中断文件
- `04_Driver`: STM32F10x 标准外设库源码和头文件
- `05_App`: 应用层模块预留
- `06_Soft`: 通用软件模块，例如延时、协议、公共工具
- `07_Source`: 工程入口文件
- `RTE/Device/STM32F103C8`: Keil RTE 配置和 `stm32f10x_conf.h`

## 工程入口

Keil 打开 `STM32F103_Base32.uvprojx`。

默认入口为 `07_Source/main.c`。CC1101 驱动已移植到 `05_App/Cc1101.c`，使用与 Lock 端相同的 SPI1 全重映射 `PB3/PB4/PB5`、`PA5` GDO0；Key 端 CSN 使用 `PB11`，避开 PB12~PB15 拨码。

## 默认编译文件

当前编译文件包括：

- `03_MCU/startup_stm32f10x_md.s`
- `03_MCU/system_stm32f10x.c`
- `03_MCU/stm32f10x_it.c`
- `04_Driver/stm32f10x_gpio.c`
- `04_Driver/stm32f10x_rcc.c`
- `04_Driver/stm32f10x_spi.c`
- `04_Driver/stm32f10x_usart.c`
- `05_App/App.c`
- `05_App/DipSwitch.c`
- `05_App/Cc1101.c`
- `05_App/OLEDI2C.c`
- `05_App/UwbParser.c`
- `05_App/UwbSerial.c`
- `06_Soft/delay.c`
- `07_Source/main.c`

## 烧录后的预期显示

```text
ID:1011
D:120.0cm
A:+4.0°
```

OLED 保留显示 PB12~PB15 的四位拨码 ID，不显示 `4E21/4E22` 的原始距离；两路距离配对后只显示圆心距离 `D`（0.1 cm）和带度数符号的 0.1° 方位角 `A`。第四行会区分 `BINDING`、`NO UART`、`BAD FRAME`、`ONE TAG`、`STALE` 和 `UART OVF`；两路距离不构成有效三角形时显示 `GEOM ERR`。

正式 CC1101 载荷为 12 字节：`D5 01 ID FLAGS DIST_L DIST_H ANGLE_L ANGLE_H SEQ_L SEQ_H QUALITY CRC8`。其字节序、硬件 CRC 和应用 CRC-8 均与 Lock 端一致。

`04_Driver` 中已放入 STM32F10x 标准外设库，后续用到串口、SPI、定时器等模块时，把对应 `.c` 加入 Keil 工程即可。
