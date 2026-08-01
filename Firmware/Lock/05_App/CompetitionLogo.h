#ifndef COMPETITION_LOGO_H
#define COMPETITION_LOGO_H

#include <stdint.h>

#define COMPETITION_LOGO_WIDTH  48u
#define COMPETITION_LOGO_HEIGHT 32u

/**
 * @brief 在指定位置绘制由根目录 icon.png 生成的大赛标识。
 * @param x 左上角横坐标。
 * @param y 左上角纵坐标。
 */
void CompetitionLogo_Draw(uint16_t x, uint16_t y);

#endif
