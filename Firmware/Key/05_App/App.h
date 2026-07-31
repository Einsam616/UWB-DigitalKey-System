#ifndef APP_H
#define APP_H

/** @brief 初始化拨码身份、UWB 串口、CC1101 和 PA4/PA6 OLED。 */
void App_Init(void);

/** @brief 轮询 UWB 帧、解算位置、发送 CC1101 定位帧并刷新 OLED。 */
void App_Run(void);

#endif
