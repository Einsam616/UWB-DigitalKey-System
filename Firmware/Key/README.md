# 数字钥匙 Key 端

STM32F103C8T6 数字钥匙端正式工程：

- `PA9` 为 STM32 USART1_TX，接 UWB Anchor 的 `RXD`；
- `PA10` 为 STM32 USART1_RX，接 UWB Anchor 的 `TXD`；
- UWB 串口参数为 `256000, 8N1`；
- 上电后程序通过 PA9 自动发送 `AT+TWLT=1,0004,4E21,4E22,FFFF,FFFF`，最多重试 5 次；
- `PA4=SCL`、`PA6=SDA` 接 128x64 SSD1306 I2C OLED；`PA5` 保留给 CC1101 GDO0；
- Key 身份由固件常量 `APP_KEY_ID=0x0D` 固定为四位二进制 `1101`，OLED 第一行显示 `ID:1101`；
- 程序分别识别 `4E21/4E22` 的两路 `mc` 距离，配对后解算圆心距离和方位角；
- CC1101 的 SPI 和 GDO0 与 Lock 端统一：SPI1 全重映射 `PB3/PB4/PB5`、GDO0=`PA5`；Key 端 CSN 使用 `PB11`。
- 有效位置以 10 Hz 封装为 12 字节帧并发送给 Lock 端，含 ID、距圆柱边界距离、方位角、序号、质量和 CRC-8。

## 烧录与测试

1. 连接 STM32、UWB Anchor、OLED 和 CC1101；CC1101 的 CSN 务必接 Key 端 `PB11`。Key 端不需要四位拨码。
2. UWB Anchor 与 STM32 必须共地；连线方向是 `Anchor TXD -> PA10`、`PA9 -> Anchor RXD`。
3. 打开 `STM32F103_Base32.uvprojx`，目标为 `EXAMPLE`，编译并烧录。
4. 将 `4E21`、`4E22` 两个 Tag 固定在门锁左右；上电时 OLED 先显示 `STATUS: BINDING`，绑定成功后 Anchor 产生包含 `aXXXX:YYYY` 身份字段的 `mc` 帧。
5. OLED 预期显示：

```text
ID:1101
D:120.0cm
A:+4.0°
STATUS: RF TX OK
```

程序在内部解析 `4E21/4E22` 的两条 `RANGE0` 距离，但 OLED 不显示原始 Tag 距离。`D` 是两圆定位得到的 Anchor 到门锁圆心距离，以 0.1 cm 显示且不做整数厘米四舍五入；`A` 以 0.1° 显示方位角，右侧为正、左侧为负；`ID` 显示本钥匙固定身份 `1101`。如制作第二把钥匙，只需为另一份固件分配不同的 4 位 `APP_KEY_ID`。

自动绑定期间显示 `STATUS: BINDING`；5 次绑定命令后仍未收到任何串口字节时显示 `STATUS: NO UART`；收到字节但没有有效 `mc` 帧时显示 `STATUS: BAD FRAME`；只收到一个 Tag 时显示 `STATUS: ONE TAG`；两路数据超出 6 秒配对窗口时显示 `STATUS: STALE`；串口环形缓冲溢出显示 `STATUS: UART OVF`；两路距离不构成有效三角形时显示 `STATUS: GEOM ERR`。有效位置发送成功显示 `STATUS: RF TX OK`，无线初始化或发送异常显示 `STATUS: RF ERROR`。

## 无线业务帧

```text
D5 01 ID FLAGS DIST_L DIST_H ANGLE_L ANGLE_H SEQ_L SEQ_H QUALITY CRC8
```

`DIST` 是圆心距离减去 `300 mm` 圆柱半径后的边界距离；`ANGLE` 单位为 `0.1 deg`；`SEQ` 每次成功发送递增；CRC-8 多项式为 `0x07`。

## 构建验证

2026-07-31 使用 Arm Compiler 5.06 update 6 对 `EXAMPLE` 目标全量重编译：`0 Error(s), 0 Warning(s)`，`Code=11832`、`RO-data=852`、`RW-data=104`、`ZI-data=2280`。生成文件为 `Objects/STM32F103_Base32.hex`。
