# 基于无线通信的数字钥匙实验系统

2026 年全国大学生电子设计竞赛 C 题项目。系统由手持数字钥匙（Key）和门锁端（Lock）组成：Key 读取 UWB 双标签距离，计算钥匙相对门锁的位置，再通过 CC1101 把身份、距离和角度发给 Lock。

## 当前状态

**基础功能已完成，当前提交为可编译、可烧录的初稿。**

- Key 端：使用固件身份 `1101`；通过 USART1 配置 UWB Anchor；解析 `4E21`、`4E22` 两个 Tag 的 `mc` 帧；使用两圆交点计算距离和方位角；OLED 显示结果；按 10 Hz 调度发送 CC1101 业务帧。
- Lock 端：接收并校验 CC1101 业务帧；读取 PA7/PB1/PB10/PB11 四位拨码作为验证 ID，PB9 为应用确认键；按距离和前方 `±45°` 区域切换等待、感应、迎宾、开锁、闭锁状态；驱动彩屏、D1～D5 指示灯、蜂鸣器和 USART1 调试输出，并支持 TM1637 手动密码开锁。
- 无线链路：Key 与 Lock 使用一致的 CC1101 频点、同步字、调制、地址和 CRC 配置；两端应用层使用一致的 12 字节协议。
- 屏幕与提示输出：Lock 屏幕使用已实测的软件 SPI 接线，启动自检关闭，初始化后直接绘制正式页面；D1～D5 为低电平点亮，PB8 蜂鸣器默认关闭。

仍需在整机上继续标定 UWB 刷新率、标签基线和角度符号，并完成外壳、电源和长期稳定性测试。上述内容不影响当前基础通信、身份验证和区域状态机验证。

## 系统数据流

```text
Key 固定 ID ─┐
             ├─ STM32F103 Key ─ USART1 ─ UWB Anchor/Tag ─ mc 解析
4E21/4E22 ───┘                                      │
                                                    ▼
                                         两圆定位：距离、角度
                                                    │
                              OLED 显示 ────────────┤
                                                    ▼
                                      CC1101 12 字节业务帧
                                                    │
                                                    ▼
       CC1101 ─ STM32F103 Lock ─ 协议/CRC/ID/区域状态机
                    ▲                 │       │       │
              Lock 四位拨码          彩屏    D1～D5   蜂鸣器
```

## 无线业务帧

CC1101 使用可变长度空口包，应用载荷固定为 12 字节。多字节字段均为小端序。

| 偏移 | 字段 | 编码 |
|---:|---|---|
| 0 | Magic | 固定 `0xD5` |
| 1 | Version | 固定 `0x01` |
| 2 | ID | 低 4 位为 Key 身份，例如 `1101`=`0x0D` |
| 3 | Flags | bit0=1 表示位置有效 |
| 4～5 | DIST | 相对门锁圆柱边界距离，单位 mm，无符号 |
| 6～7 | ANGLE | 方位角，单位 `0.1°`，有符号 |
| 8～9 | SEQ | 连续帧序号 |
| 10 | QUALITY | 定位质量，当前基础版本为 `100` |
| 11 | CRC8 | CRC-8，多项式 `0x07`，覆盖偏移 0～10 |

Lock 端还保留 ASCII `666` 诊断帧入口，但该帧只显示无线测试状态，不触发开锁。

## 区域和身份判定

- Key 当前固定发送 ID `1101`（代码中的 `APP_KEY_ID=0x0D`）；Lock 的 PA7/PB1/PB10/PB11 设置待验证 ID，PB9 确认应用，ON 接地表示 `1`，从左到右对应 bit3～bit0。
- Lock 拨成 `1101` 时身份通过；任意一位不一致时保留测距显示，但保持闭锁。Lock 运行中检测到拨码变化后立即显示修改状态，按 PB9 应用后重新判定当前钥匙。
- 角度超出前方 `±45°` 时判为区域外。
- 相对圆柱边界距离：`0～1 m` 开锁区，`1～2 m` 迎宾区，`2～3 m` 感应区，超过 `3 m` 闭锁。
- 各边界使用 `50 mm` 回差；连续超过 `3 s` 没有有效帧时自动闭锁，上一组定位数字以灰色保留，恢复通信后直接原位更新。
- Lock 指示灯：D1 闭锁/错误、D2 开锁、D3 检测到钥匙、D4 迎宾区、D5 身份通过。

## 关键硬件 IO

### Key

| 功能 | 引脚 |
|---|---|
| UWB USART1 TX/RX | PA9 / PA10 |
| OLED 软件 I2C SCL/SDA | PA4 / PA6 |
| CC1101 GDO0 | PA5 |
| CC1101 SPI1（全重映射）SCK/MISO/MOSI | PB3 / PB4 / PB5 |
| CC1101 CSN | PB12 |
| Key 身份 ID | 固件常量 `APP_KEY_ID=1101`，不占 GPIO |

### Lock

| 功能 | 引脚 |
|---|---|
| D1～D5 指示灯 | PA0～PA4，低电平点亮 |
| CC1101 GDO0 | PA5 |
| 四位验证 ID（从左到右） | PA7 / PB1 / PB10 / PB11 |
| ID 应用确认键 | PB9 |
| 彩屏 CS1/RES/DC/CS2 | PA8 / PA11 / PA12 / PA15 |
| 彩屏 BLK | PB0 |
| CC1101 SPI1（全重映射）SCK/MISO/MOSI | PB3 / PB4 / PB5 |
| TM1637 CLK/IO（预留） | PB6 / PB7 |
| 蜂鸣器 PWM | PB8，TIM4_CH3 |
| CC1101 CSN | PB12 |
| 彩屏 SPI 软件 CLK/MISO/MOSI | PB13 / PB14 / PB15 |
| 无线接收活动探针（当前） | PC13 |

PA13/PA14 保留 SWD 下载调试，PB2 保持 BOOT1 下拉。Key 与 Lock 的 CC1101 均使用 PB12 作为本地 CSN；Lock 的 PC13 当前输出接收活动探针，启用触摸时再恢复为 PEN 输入。

## 工程和目录

```text
Firmware/
├── Key/                         数字钥匙端
│   ├── STM32F103_Base32.uvprojx
│   ├── 01_Doc/                  接线、库和工程说明
│   ├── 05_App/                  UWB、OLED、CC1101 应用模块
│   └── 06_Soft/                 两圆定位和延时
└── Lock/                        门锁端
    ├── UWB_DigitalKey_Lock.uvprojx
    ├── 01_Doc/                  接线、UI 和协议说明
    ├── 05_App/                  彩屏、拨码、IO、CC1101 应用模块
    └── 06_Soft/                 协议解码和区域状态机
```

## Keil 构建与烧录

环境：Keil MDK 5、Arm Compiler 5.06 update 6、STM32F10x Standard Peripheral Library、目标芯片 STM32F103C8T6。

1. 用 Keil 打开 `Firmware/Key/STM32F103_Base32.uvprojx` 或 `Firmware/Lock/UWB_DigitalKey_Lock.uvprojx`。
2. 在 `Options for Target -> Output` 勾选 `Create HEX File`。
3. 选择 `Rebuild`，HEX 默认输出到对应工程的 `Objects/` 目录。
4. 连接 ST-Link，确认目标芯片为 STM32F103C8，再执行 Download。

命令行构建示例：

```powershell
E:\Keli5\UV4\UV4.exe -r "Firmware\Key\STM32F103_Base32.uvprojx" -j0 -o "Firmware\Key\key-build.log"
E:\Keli5\UV4\UV4.exe -r "Firmware\Lock\UWB_DigitalKey_Lock.uvprojx" -j0 -o "Firmware\Lock\lock-build.log"
```

本地构建日志、HEX、Keil 用户状态、临时测试工程、原始赛题和分析资料由 `.gitignore` 排除，不属于初稿发布内容。

## 发布范围

GitHub 初稿只保留 Key/Lock 正式工程、必要的工程说明、接线表和协议说明。`docs/`、`Firmware/Tests/`、抓包记录、屏幕字库预览图、构建日志和编译输出留在本地，避免把阶段性资料混入正式工程。

## 相关文档

- [Firmware 工程索引](Firmware/README.md)
- [Key 端说明](Firmware/Key/README.md)
- [Lock 端说明](Firmware/Lock/01_Doc/README.md)
- [Lock 接线表](Firmware/Lock/01_Doc/PinMap.md)
- [Lock UI 和状态说明](Firmware/Lock/01_Doc/UI.md)
- [12 字节协议测试向量](Firmware/Lock/01_Doc/LockPacketTestVectors.md)
