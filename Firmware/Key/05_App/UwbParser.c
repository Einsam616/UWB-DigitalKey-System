#include "UwbParser.h"

#define UWB_BIND_ACK_LENGTH 9u

static const uint8_t s_bind_ack[UWB_BIND_ACK_LENGTH] =
{
    'O', 'K', '+', 'T', 'W', 'L', 'T', '=', '1'
};

/** @brief Reset only the current mc line without clearing command state. */
static void UwbParser_ResetLine(UwbLineParser *parser)
{
    parser->length = 0u;
    parser->line[0] = '\0';
}

/** @brief Match the binding acknowledgement in the unfiltered byte stream. */
static void UwbParser_ConsumeBindAck(UwbLineParser *parser, uint8_t value)
{
    if (value == s_bind_ack[parser->bind_ack_index])
    {
        parser->bind_ack_index++;
        if (parser->bind_ack_index >= UWB_BIND_ACK_LENGTH)
        {
            parser->bind_ack = 1u;
            parser->bind_ack_index = 0u;
        }
    }
    else
    {
        parser->bind_ack_index = (value == 'O') ? 1u : 0u;
    }
}

/** @brief Convert one hexadecimal character to its numeric value. */
static int UwbParser_Hex(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

/** @brief Parse a fixed-width hexadecimal field. */
static uint32_t UwbParser_HexField(const char *text, uint8_t digits, uint8_t *valid)
{
    uint8_t index;
    uint32_t value = 0u;

    *valid = 0u;
    for (index = 0u; index < digits; ++index)
    {
        int digit = UwbParser_Hex(text[index]);
        if (digit < 0) return 0u;
        value = (value << 4u) | (uint32_t)digit;
    }
    *valid = 1u;
    return value;
}

/** @brief Return 1 for protocol field separators. */
static uint8_t UwbParser_IsSeparator(char value)
{
    return (value == ' ' || value == '\t') ? 1u : 0u;
}

/** @brief Parse RANGE0 and aXXXX:YYYY from one complete mc line. */
static uint8_t UwbParser_ParseLine(const char *line,
                                   uint8_t length,
                                   UwbMeasurement *measurement)
{
    const char *cursor;
    const char *end;
    const char *token_start;
    uint8_t valid;
    uint32_t range;
    uint32_t tag;

    if (length < 3u || line[0] != 'm' || line[1] != 'c' ||
        UwbParser_IsSeparator(line[2]) == 0u)
    {
        return 0u;
    }

    cursor = &line[2];
    end = &line[length];

    /* Skip separators and the MASK field. */
    while (cursor < end && UwbParser_IsSeparator(*cursor) != 0u) cursor++;
    token_start = cursor;
    while (cursor < end && UwbParser_IsSeparator(*cursor) == 0u) cursor++;
    if (cursor == token_start) return 0u;

    /* RANGE0 is the six-digit hexadecimal field immediately after MASK. */
    while (cursor < end && UwbParser_IsSeparator(*cursor) != 0u) cursor++;
    if ((end - cursor) < 6) return 0u;
    range = UwbParser_HexField(cursor, 6u, &valid);
    if (valid == 0u || range == 0u) return 0u;
    if ((cursor + 6) < end && UwbParser_IsSeparator(cursor[6]) == 0u) return 0u;
    cursor += 6;

    /* Tag position varies between firmware formats, so scan all later fields. */
    while (cursor < end)
    {
        while (cursor < end && UwbParser_IsSeparator(*cursor) != 0u) cursor++;
        token_start = cursor;
        while (cursor < end && UwbParser_IsSeparator(*cursor) == 0u) cursor++;
        if ((cursor - token_start) >= 6 &&
            (token_start[0] == 'a' || token_start[0] == 'A') &&
            token_start[5] == ':')
        {
            tag = UwbParser_HexField(&token_start[1], 4u, &valid);
            if (valid != 0u)
            {
                measurement->tag_id = (uint16_t)tag;
                measurement->range0_mm = range;
                return 1u;
            }
        }
    }
    return 0u;
}

/** @brief Reset the streaming line parser. */
void UwbParser_Reset(UwbLineParser *parser)
{
    if (parser != 0)
    {
        UwbParser_ResetLine(parser);
        parser->bind_ack_index = 0u;
        parser->bind_ack = 0u;
    }
}

/** @brief Consume one byte and emit a measurement for a complete mc line. */
uint8_t UwbParser_Consume(UwbLineParser *parser,
                          uint8_t value,
                          UwbMeasurement *measurement)
{
    uint8_t parsed;

    if (parser == 0 || measurement == 0) return 0u;
    UwbParser_ConsumeBindAck(parser, value);
    if (value == '\r' || value == '\n')
    {
        parsed = UwbParser_ParseLine(parser->line, parser->length, measurement);
        UwbParser_ResetLine(parser);
        return parsed;
    }
    if (value == 0u)
    {
        UwbParser_ResetLine(parser);
        return 0u;
    }

    /* Ignore binary startup noise until an mc header starts. */
    if (parser->length == 0u)
    {
        if (value != 'm') return 0u;
    }
    else if (parser->length == 1u && parser->line[0] == 'm' && value != 'c')
    {
        parser->length = 0u;
        if (value != 'm') return 0u;
    }
    else if (parser->length == 2u && UwbParser_IsSeparator((char)value) == 0u)
    {
        parser->length = 0u;
        if (value != 'm') return 0u;
    }
    if (parser->length >= (uint8_t)(sizeof(parser->line) - 1u))
    {
        UwbParser_ResetLine(parser);
        if (value != 'm') return 0u;
    }
    parser->line[parser->length++] = (char)value;
    return 0u;
}

/** @brief Return and clear one latched binding acknowledgement. */
uint8_t UwbParser_TakeBindAck(UwbLineParser *parser)
{
    uint8_t bind_ack;

    if (parser == 0) return 0u;
    bind_ack = parser->bind_ack;
    parser->bind_ack = 0u;
    return bind_ack;
}
