# 门锁工程库清单

| 库/模块 | 来源 | 用途 | 工程文件 | 适配说明 |
|---|---|---|---|---|
| CC1101 无线 | `E:\Work\Competition\电赛\C1101试验\02_Receiver\05_App` | 已测试的 400 MHz GFSK 数据链路 | `05_App/Cc1101.c`、`Cc1101.h`、`Cc1101Regs.h` | SPI1 全重映射，PA5 接 GDO0，PB12 接 CSN |
| 彩屏驱动 | `E:\Work\单片机\库\Screen 屏幕` | 320x240 SPI 彩屏和 ASCII 绘制 | `05_App/Screen.c`、`Screen.h` | CS1=PA8、CS2=PA15、RES=PA11、DC=PA12、BLK=PB0、CLK=PB13、MOSI=PB15，保留库的软件 SPI 与通用初始化 |
| 中文字库 | `E:\Work\Competition\电赛\2025非接触式控制盘\05_App` | UTF-8 中文绘制 | `05_App/ChineseFont.c`、`ChineseFont.h`、`ChineseFont16Data.h`、`ChineseFont24Data.h` | 按门锁界面文本独立生成 16x16 正文和 24x24 标题 |
| Key 端 ID 输入 | 本仓库 `Firmware/Key/05_App` | 四位发射身份设定 | Key 工程 `DipSwitch.c`、`DipSwitch.h` | PB12～PB15 的拨码按 `ID[3:0]` 组成发射帧字节 2 |
| Lock 端 ID 输入 | 本工程 | 四位接收身份设定 | `05_App/DipSwitch.c`、`DipSwitch.h` | PA6/PA7/PB9/PB10 对应 `ID[3:0]`，运行中变化后立即重新判定 |
| TM1637 | `E:\Work\Competition\电赛\2025非接触式控制盘\05_App` | Lock 端预留键盘/显示接口 | `05_App/TM1637Keypad.c`、`TM1637Keypad.h` | 已适配 PB6/PB7 并保留在工程，当前版本不初始化 |
| 延时 | 锁工程基础框架 | 微秒/毫秒延时 | `06_Soft/delay.c`、`delay.h` | 保留框架实现 |
| 调试串口 | 工程内实现 | USART1 数据和状态日志 | `05_App/DebugSerial.c`、`DebugSerial.h` | STM32F103 轮询发送，115200 8N1 |
| 数据帧 | 工程内实现 | 协议、CRC-8 和字段解码 | `06_Soft/LockPacket.c`、`LockPacket.h` | 12 字节正式门锁定位帧 |
| 状态机 | 工程内实现 | 感应区、迎宾区、开锁区和超时闭锁 | `06_Soft/LockState.c`、`LockState.h` | 1 m/2 m/3 m 边界使用 50 mm 回差 |

彩屏页面使用 320x240 中文仪表盘：顶部显示开闭锁状态，中部显示接收 ID、拨码设定 ID、距离、角度，底部显示区域、质量、丢包和最近事件。驱动沿用公共库的 GPIO 软件 SPI；旧的 PA4/PA5 软件 I2C OLED 文件仅保留作参考，不加入本目标。
