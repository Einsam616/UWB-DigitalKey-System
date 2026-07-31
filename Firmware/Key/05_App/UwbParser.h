#ifndef UWB_PARSER_H
#define UWB_PARSER_H

#include <stdint.h>

typedef struct
{
    uint16_t tag_id;
    uint32_t range0_mm;
} UwbMeasurement;

typedef struct
{
    char line[128];
    uint8_t length;
} UwbLineParser;

/** @brief 清空 UWB 行解析器。 */
void UwbParser_Reset(UwbLineParser *parser);

/**
 * @brief 输入一个字节并解析完整的 mc 行。
 * @param parser 行解析状态。
 * @param value 新字节。
 * @param measurement 成功时输出 Tag ID 和 RANGE0 毫米值。
 * @return 得到有效测距行返回 1，否则返回 0。
 */
uint8_t UwbParser_Consume(UwbLineParser *parser,
                          uint8_t value,
                          UwbMeasurement *measurement);

#endif
