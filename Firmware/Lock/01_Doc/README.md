# STM32F103 基础工程框架

本工程目录参考 `embedded-operating-system-STM32F1` 整理，用于 STM32F103C8。

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

Keil 打开 `UWB_DigitalKey_Lock.uvprojx`。

默认入口为 `07_Source/main.c`，当前只保留最小空循环和延时调用，方便继续添加业务功能。

## 默认编译文件

工程默认只编译基础启动和最小外设依赖：

- `03_MCU/startup_stm32f10x_md.s`
- `03_MCU/system_stm32f10x.c`
- `03_MCU/stm32f10x_it.c`
- `04_Driver/misc.c`
- `04_Driver/stm32f10x_flash.c`
- `04_Driver/stm32f10x_gpio.c`
- `04_Driver/stm32f10x_rcc.c`
- `06_Soft/delay.c`
- `07_Source/main.c`

`04_Driver` 中已放入 STM32F10x 标准外设库，后续用到串口、SPI、定时器等模块时，把对应 `.c` 加入 Keil 工程即可。
