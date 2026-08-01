# 数字钥匙 Key 端工程说明

本工程用于 STM32F103C8T6，PA9/PA10 以 `256000, 8N1` 自动绑定 `4E21/4E22` 并接收 UWB Anchor 的 `mc` 帧。每条有效距离都立即更新并参与定位，不做多点平均；CC1101 每 100 ms（10 Hz）发送给 Lock 端。PA4/PA6 驱动 OLED，PA5 用于 CC1101 GDO0。电源由自锁开关 `SW_PWR` 控制，开关后级并联 `R_LED + LED_PWR`；上电复位后固件自动运行，LED 不占用 MCU GPIO。

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

默认入口为 `07_Source/main.c`。CC1101 驱动已移植到 `05_App/Cc1101.c`，使用与 Lock 端相同的 SPI1 全重映射 `PB3/PB4/PB5`、`PA5` GDO0；Key 端 CSN 使用 `PB12`。钥匙身份由 `App.c` 中的 `APP_KEY_ID` 固定配置，当前为 `1101`。

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
- `05_App/Cc1101.c`
- `05_App/OLEDI2C.c`
- `05_App/UwbParser.c`
- `05_App/UwbSerial.c`
- `06_Soft/delay.c`
- `07_Source/main.c`

## 烧录后的预期显示

```text
ID:1101
D:120.0cm
A:+4.0°
```

OLED 显示 Key 固定身份 `1101`，不显示 `4E21/4E22` 的原始距离；每条有效距离都立即更新，不做多点平均。只有到达时间相差不超过 250 ms 的左右帧才组成定位对；OLED 每 100 ms 刷新并优先于 CC1101 发送执行。OLED 上电等待约 100 ms 后发送绑定命令；识别到 `OK+TWLT=1` 后停止重发，否则每 2.5 秒重试、最多 5 次；启动横幅和绑定应答期间保持 `BINDING`，不误报 `BAD FRAME`。数据有效期为 6 秒；第四行还会区分 `NO UART`、`ONE TAG`、`STALE`、`UART OVF` 和 `GEOM ERR`。

正式 CC1101 载荷为 12 字节：`D5 01 ID FLAGS DIST_L DIST_H ANGLE_L ANGLE_H SEQ_L SEQ_H QUALITY CRC8`。发送周期为 100 ms；其字节序、硬件 CRC 和应用 CRC-8 均与 Lock 端一致。

`04_Driver` 中已放入 STM32F10x 标准外设库，后续用到串口、SPI、定时器等模块时，把对应 `.c` 加入 Keil 工程即可。
