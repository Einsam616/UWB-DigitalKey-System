# Firmware 工程索引

仓库当前发布两套正式 STM32F103C8T6 工程。两端使用一致的 CC1101 空口参数和 12 字节应用帧。

## CC1101 独立收发测试工程

已从正式 Key/Lock 工程中剥离出最小点对点测试版本：

| 端 | Keil 工程 | 行为 |
|---|---|---|
| 发射端 | `CC1101_Transmitter/CC1101_Demo.uvprojx` | 每 500 ms 发送 `CC1101TX`，PC13 成功发送后翻转 |
| 接收端 | `CC1101_Receiver/CC1101_Demo.uvprojx` | 轮询合法 CRC 包，PC13 每收到一包翻转 |

两端公共 CC1101 库位于 `05_App/`，公共备份同时放在 `E:\Work\单片机\库\CC1101 STM32F103`。库的板级配置集中在 `Cc1101_Config.h`。

| 端 | Keil 工程 | HEX 输出 |
|---|---|---|
| 数字钥匙 Key | `Key/STM32F103_Base32.uvprojx` | `Key/Objects/STM32F103_Base32.hex` |
| 门锁 Lock | `Lock/UWB_DigitalKey_Lock.uvprojx` | `Lock/Objects/UWB_DigitalKey_Lock.hex` |

## Key

Key 端通过 PA9/PA10 与 UWB Anchor 通信，绑定 `4E21`、`4E22` 两个 Tag，使用 500 mm 标签基线进行两圆定位。Key 身份由固件常量 `APP_KEY_ID` 固定为 `1101`，PA4/PA6 驱动 OLED，CC1101 使用 SPI1 全重映射和 PB12 CSN。

入口：[`Key/README.md`](Key/README.md)

## Lock

Lock 端接收 Key 的 CC1101 业务帧，用 PA7/PB1/PB10/PB11 四位拨码设置验证 ID，PB9 确认应用，依据 1 m/2 m/3 m 边界和前方 `±45°` 区域控制彩屏、D1～D5 和蜂鸣器，并支持 TM1637 密码锁。屏幕使用 PB13/PB15 软件 SPI，CS1 为 PA8，CS2 为 PA15 并保持高电平。

入口：[`Lock/01_Doc/README.md`](Lock/01_Doc/README.md)

## 构建

使用 Keil MDK 5 + Arm Compiler 5.06 update 6。打开任一 `.uvprojx`，在 `Options for Target -> Output` 勾选 `Create HEX File` 后执行 `Rebuild`。构建输出和用户状态文件被 `.gitignore` 排除。

仓库不包含阶段性抓包、临时测试工程、原始题目 PDF 和本地构建日志。
