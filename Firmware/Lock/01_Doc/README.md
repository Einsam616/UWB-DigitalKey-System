# 数字钥匙门锁

本工程是 C 题 STM32F103C8T6 门锁端原型。它接收正式 CC1101 定位帧，校验钥匙 ID，依据 60 cm 圆柱边界和前方 ±45° 区域做判定，并驱动门锁端提示输出。彩屏使用中文仪表界面，不包含账号、密码和登录流程。

## 构建入口

- Keil 工程：`UWB_DigitalKey_Lock.uvprojx`
- 编译器：Arm Compiler 5 / STM32F10x Standard Peripheral Library
- 程序入口：`07_Source/main.c`

## 运行路径

1. CC1101 接收无线应用帧。
2. 数据帧解码器检查魔数、版本和 CRC-8。
3. 接收帧中的 `KEY_ID` 与 Lock 端 PA6/PA7/PB9/PB10 四位拨码设置值比较；Key 或 Lock 任一端拨码改变后，Lock 实时显示并重新判定。
4. 检查相对 60 cm 圆柱边界的径向距离和有符号角度。
5. 状态机对 1 m、2 m 和 3 m 区域边界使用 50 mm 回差。
6. 彩屏显示 ID、距离、角度、区域、锁状态和无线质量。
7. USART1 以 115200 8N1 输出解码帧与超时闭锁事件。

## 当前 Lock 端 IO 行为

- PA0～PA4 是 D1～D5 低电平点亮指示灯：运行、无线链路、迎宾区、闭锁、开锁。
- PA6、PA7、PB9、PB10 是 Lock 端四位验证 ID 输入，从左到右对应 bit3～bit0；内部上拉，ON 接地为 `1`。
- PB8 是低有效蜂鸣器 PWM，初始化先保持关闭，再按状态机事件短鸣。
- PB6/PB7 的 TM1637 接线和驱动文件保留，但当前版本不初始化、不参与 ID 设置。
- 彩屏沿用旧项目实测的软件 SPI 和初始化时序，使用 PB13/PB15，PB14 保留；CS1/RES/DC/CS2 分别为 PA8/PA11/PA12/PA15。初始化后直接进入正式页面。

## 应用帧

正式 CC1101 载荷为 12 字节：

```text
D5 01 ID FLAGS DIST_L DIST_H ANGLE_L ANGLE_H SEQ_L SEQ_H QUALITY CRC8
```

`DIST` 是相对于 60 cm 圆柱边界的径向距离，单位 mm；`ANGLE` 是角度乘以 10 后的有符号整数；CRC-8 覆盖字节 0～10。ASCII 载荷 `666` 仍保留用于无线冒烟测试，但不会触发开锁。

## 硬件提示

- PB8 使用 TIM4_CH3 硬件 PWM，频率 2 kHz，占空比 50%；迎宾和开锁提示采用非阻塞定时。
- PC13 保留为触摸 PEN 中断，本目标不把它当作 LED；彩屏颜色和 D1～D5、蜂鸣器共同作为门锁提示。
- 无 CC1101 模块时，彩屏仍可启动，并显示“模块异常”。

## 构建验证

- 2026-07-31 使用 Arm Compiler 5.06 update 6 全量重建：`0 Error(s), 0 Warning(s)`。
- 程序容量：`Code=15508`、`RO-data=3412`、`RW-data=88`、`ZI-data=1632`。
- 输出文件：`Objects/UWB_DigitalKey_Lock.hex`。
- 彩屏最终接线和中文页面已完成实物显示确认。

## 屏幕最终配置

- 正式固件已关闭红、绿、蓝启动自检，屏幕初始化后直接绘制门锁页面。
- 屏幕使用 `CS1=PA8`、`RES=PA11`、`DC=PA12`、`CLK=PB13`、`MOSI=PB15`、`BLK=PB0`。
- `CS2=PA15` 保持高电平，`MISO=PB14` 和 `PEN=PC13` 当前保留。
