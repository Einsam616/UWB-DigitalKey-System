# 门锁工程库清单

| 库/模块 | 来源 | 用途 | 工程文件 | 适配说明 |
|---|---|---|---|---|
| CC1101 无线 | `E:\Work\Competition\电赛\C1101试验\02_Receiver\05_App` | 已测试的 400 MHz GFSK 数据链路 | `05_App/Cc1101.c`、`Cc1101.h`、`Cc1101Regs.h` | SPI1 全重映射，PA5 接 GDO0，PB12 接 CSN |
| 彩屏驱动 | `E:\Work\单片机\库\Screen 屏幕` | 320x240 SPI 彩屏和 ASCII 绘制 | `05_App/Screen.c`、`Screen.h` | CS1=PA8、CS2=PA15、RES=PA11、DC=PA12、BLK=PB0、CLK=PB13、MOSI=PB15；增加单窗口连续单色点阵写入，供动态中文局部刷新 |
| 中文字库 | `E:\Work\Competition\电赛\2025非接触式控制盘\05_App` | UTF-8 中文绘制 | `05_App/ChineseFont.c`、`ChineseFont.h`、`ChineseFont16Data.h`、`ChineseFont24Data.h` | 按门锁界面文本独立生成 16x16 正文和 24x24 标题 |
| Lock 端 ID 输入 | 本工程 | 四位待验证身份设定和确认键 | `05_App/DipSwitch.c`、`DipSwitch.h` | PA7/PB1/PB10/PB11 对应 `ID[3:0]`，PB9 确认；内部上拉且 ON/按下接 GND 为 1 |
| 竞赛标识 | 本工程生成资源 | 门锁页 48x32 标识 | `05_App/CompetitionLogo.c`、`CompetitionLogo.h` | RGB565 位图，随正式界面编译 |
| TM1637 | `E:\Work\Competition\电赛\2025非接触式控制盘\05_App` | Lock 端 4x4 键盘扫描和 P/F 状态显示 | `05_App/TM1637Keypad.c`、`TM1637Keypad.h` | 适配 PB6/PB7；复用 TM1637 读键时序，增加 0x40/0xC0 显示写入 |
| 密码存储 | 本工程 | 四位密码的 Flash 持久化和校验 | `05_App/PasswordStore.c`、`PasswordStore.h` | 使用 STM32F103C8 最后一页 `0x0800FC00`，魔数、反码和数字范围三重校验 |
| 延时 | 锁工程基础框架 | 微秒/毫秒延时 | `06_Soft/delay.c`、`delay.h` | 保留框架实现 |
| 调试串口 | 工程内实现 | USART1 数据和状态日志 | `05_App/DebugSerial.c`、`DebugSerial.h` | STM32F103 轮询发送，115200 8N1 |
| 数据帧 | 工程内实现 | 协议、CRC-8 和字段解码 | `06_Soft/LockPacket.c`、`LockPacket.h` | 12 字节正式门锁定位帧 |
| 状态机 | 工程内实现 | 感应区、迎宾区、开锁区和超时闭锁 | `06_Soft/LockState.c`、`LockState.h` | 1 m/2 m/3 m 边界使用 50 mm 回差；通信超时 3 s，超时保留上一组数据显示 |

彩屏页面使用 320x240 中文仪表盘：顶部显示开闭锁状态，中部显示接收 ID、拨码设定 ID、径向距离、方位角，底部显示区域、通信状态和最近事件。静态布局只在初始化时绘制一次，运行时按字段缓存使用局部 LCD 窗口更新；旧的 PA4/PA5 软件 I2C OLED 文件仅保留作参考，不加入本目标。

Key 当前由 `Firmware/Key/05_App/App.c` 中的 `APP_KEY_ID=0x0D` 固定发送 `1101`，Lock 端拨码只负责设置接收验证 ID。
