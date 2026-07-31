#include "ChineseFont.h"

#include "ChineseFont16Data.h"
#include "ChineseFont24Data.h"
#include "Screen.h"

#define CHINESE_FONT_Y_OFFSET 1u

static uint32_t ChineseFont_DecodeUtf8(const char **text)
{
    const uint8_t *bytes = (const uint8_t *)*text;
    uint32_t codepoint;

    if (bytes[0] < 0x80u) {
        (*text)++;
        return bytes[0];
    }
    if (((bytes[0] & 0xE0u) == 0xC0u) &&
        ((bytes[1] & 0xC0u) == 0x80u)) {
        codepoint = ((uint32_t)(bytes[0] & 0x1Fu) << 6) |
                    (uint32_t)(bytes[1] & 0x3Fu);
        *text += 2;
        return codepoint;
    }
    if (((bytes[0] & 0xF0u) == 0xE0u) &&
        ((bytes[1] & 0xC0u) == 0x80u) &&
        ((bytes[2] & 0xC0u) == 0x80u)) {
        codepoint = ((uint32_t)(bytes[0] & 0x0Fu) << 12) |
                    ((uint32_t)(bytes[1] & 0x3Fu) << 6) |
                    (uint32_t)(bytes[2] & 0x3Fu);
        *text += 3;
        return codepoint;
    }
    (*text)++;
    return (uint32_t)'?';
}

static const uint8_t *ChineseFont_FindGlyph16(uint32_t codepoint)
{
    uint16_t index;
    for (index = 0u; index < CHINESE_FONT16_GLYPH_COUNT; index++) {
        if (ChineseFont16_Glyphs[index].codepoint == codepoint) {
            return ChineseFont16_Glyphs[index].bitmap;
        }
    }
    return 0;
}

static const uint8_t *ChineseFont_FindGlyph24(uint32_t codepoint)
{
    uint16_t index;
    for (index = 0u; index < CHINESE_FONT24_GLYPH_COUNT; index++) {
        if (ChineseFont24_Glyphs[index].codepoint == codepoint) {
            return ChineseFont24_Glyphs[index].bitmap;
        }
    }
    return 0;
}

static uint8_t ChineseFont_IsPixelOn(const uint8_t *bitmap,
                                     uint8_t width,
                                     uint8_t row,
                                     uint8_t column)
{
    uint8_t bytes_per_row = (uint8_t)((width + 7u) / 8u);
    uint8_t byte_value = bitmap[(uint16_t)row * bytes_per_row +
                                column / 8u];
    return (uint8_t)((byte_value &
                      (uint8_t)(0x80u >> (column % 8u))) != 0u);
}

static void ChineseFont_DrawGlyph(uint16_t x, uint16_t y,
                                  const uint8_t *bitmap,
                                  uint8_t width, uint8_t height,
                                  uint16_t text_color,
                                  uint16_t back_color,
                                  uint8_t scale)
{
    uint8_t row;
    uint8_t column;
    uint8_t run_start;

    if (scale == 0u) scale = 1u;
    Screen_FillRect(x, y, (uint16_t)(width * scale),
                    (uint16_t)(height * scale), back_color);
    if (bitmap == 0) {
        Screen_DrawRect(x, y, (uint16_t)(width * scale),
                        (uint16_t)(height * scale), text_color);
        return;
    }
    for (row = 0u; row < height; row++) {
        column = 0u;
        while (column < width) {
            while ((column < width) &&
                   (ChineseFont_IsPixelOn(bitmap, width, row, column) == 0u)) {
                column++;
            }
            run_start = column;
            while ((column < width) &&
                   (ChineseFont_IsPixelOn(bitmap, width, row, column) != 0u)) {
                column++;
            }
            if (column > run_start) {
                Screen_FillRect((uint16_t)(x + (uint16_t)run_start * scale),
                                (uint16_t)(y + (uint16_t)row * scale),
                                (uint16_t)((column - run_start) * scale),
                                scale, text_color);
            }
        }
    }
}

void ChineseFont_ShowText(uint16_t x, uint16_t y,
                          const char *text,
                          uint16_t text_color,
                          uint16_t back_color,
                          uint8_t scale)
{
    uint16_t cursor_x = x;
    uint16_t draw_y = (uint16_t)(y + CHINESE_FONT_Y_OFFSET);
    uint32_t codepoint;
    const uint8_t *bitmap;

    if ((text == 0) || (*text == '\0')) return;
    if (scale == 0u) scale = 1u;
    while (*text != '\0') {
        codepoint = ChineseFont_DecodeUtf8(&text);
        if (codepoint < 0x80u) {
            Screen_ShowChar16(cursor_x, draw_y, (char)codepoint,
                              text_color, back_color, scale);
            cursor_x = (uint16_t)(cursor_x + 6u * scale);
        } else {
            bitmap = ChineseFont_FindGlyph16(codepoint);
            ChineseFont_DrawGlyph(cursor_x, draw_y, bitmap, 16u, 16u,
                                  text_color, back_color, scale);
            cursor_x = (uint16_t)(cursor_x + 16u * scale);
        }
    }
}

void ChineseFont_ShowTitle24(uint16_t x, uint16_t y,
                             const char *text,
                             uint16_t text_color,
                             uint16_t back_color)
{
    uint16_t cursor_x = x;
    uint16_t draw_y = (uint16_t)(y + CHINESE_FONT_Y_OFFSET);
    uint32_t codepoint;
    const uint8_t *bitmap;

    if (text == 0) return;
    while (*text != '\0') {
        codepoint = ChineseFont_DecodeUtf8(&text);
        if (codepoint < 0x80u) {
            Screen_ShowChar16(cursor_x, (uint16_t)(draw_y + 4u),
                              (char)codepoint, text_color, back_color, 1u);
            cursor_x = (uint16_t)(cursor_x + 6u);
        } else {
            bitmap = ChineseFont_FindGlyph24(codepoint);
            ChineseFont_DrawGlyph(cursor_x, draw_y, bitmap, 24u, 24u,
                                  text_color, back_color, 1u);
            cursor_x = (uint16_t)(cursor_x + 24u);
        }
    }
}
