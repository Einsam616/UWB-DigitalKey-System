#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "UwbParser.h"

typedef struct
{
    uint16_t tag_id;
    uint32_t range0_mm;
} ExpectedMeasurement;

static int Replay(const uint8_t *stream,
                  size_t length,
                  const ExpectedMeasurement *expected,
                  size_t expected_count)
{
    UwbLineParser parser;
    UwbMeasurement measurement;
    size_t index;
    size_t parsed = 0u;

    UwbParser_Reset(&parser);
    for (index = 0u; index < length; ++index)
    {
        if (UwbParser_Consume(&parser, stream[index], &measurement) != 0u)
        {
            if (parsed >= expected_count)
            {
                fprintf(stderr, "unexpected measurement %04X:%lu\n",
                        measurement.tag_id,
                        (unsigned long)measurement.range0_mm);
                return 1;
            }
            if (measurement.tag_id != expected[parsed].tag_id ||
                measurement.range0_mm != expected[parsed].range0_mm)
            {
                fprintf(stderr,
                        "measurement %lu: got %04X:%lu, expected %04X:%lu\n",
                        (unsigned long)parsed,
                        measurement.tag_id,
                        (unsigned long)measurement.range0_mm,
                        expected[parsed].tag_id,
                        (unsigned long)expected[parsed].range0_mm);
                return 1;
            }
            parsed++;
        }
    }

    if (parsed != expected_count)
    {
        fprintf(stderr, "parsed %lu measurements, expected %lu\n",
                (unsigned long)parsed, (unsigned long)expected_count);
        return 1;
    }
    return 0;
}

int main(void)
{
    UwbLineParser ack_parser;
    UwbMeasurement ack_measurement;
    size_t ack_index;
    static const uint8_t bind_ack_stream[] =
        "startup noise\r\nOK+TWLT=1\r\n";
    static const uint8_t captured_stream[] =
        "\x00\xFF\x83\x19startup\x00noise\r\n"
        "mc 01 0004b5 000000 000000 000000 000000 000000 008a bb 0002302c a4e21:0000 0 0c00 5c2d 038d\r\n"
        "mc 01 0004ba 000000 000000 000000 000000 000000 008b bf 00023054 a4e22:0000 0 0c0c 5942 0383\r\n"
        "mc 01 00050e 000000 000000 000000 000000 000000 008c bc 00023824 a4e21:0000 0 0c00 5d80 038e\r\n"
        "mc 01 0004a7 000000 000000 000000 000000 000000 008d c0 0002384c a4e22:0000 0 0c0c 5c0a 0395\r\n"
        "mc 01 0004f7 000000 000000 000000 000000 000000 008e bd 0002401c a4e21:0000 0 0c00 5c15 03b4\r\n"
        "mc 01 0004ed 000000 000000 000000 000000 000000 008f c1 00024044 a4e22:0000 0 0c0c 6244 03b0\r\n";
    static const ExpectedMeasurement expected[] =
    {
        {0x4E21u, 0x0004B5u},
        {0x4E22u, 0x0004BAu},
        {0x4E21u, 0x00050Eu},
        {0x4E22u, 0x0004A7u},
        {0x4E21u, 0x0004F7u},
        {0x4E22u, 0x0004EDu}
    };
    static const uint8_t extended_frame[] =
        "mc 01 0004b5 0 0 0 0 0 0 0 0 0 0 0 0 0 0 a4e21:0000 0 0c00 5c2d 038d\r\n";
    static const ExpectedMeasurement extended_expected[] =
    {
        {0x4E21u, 0x0004B5u}
    };

    UwbParser_Reset(&ack_parser);
    for (ack_index = 0u; ack_index < sizeof(bind_ack_stream) - 1u;
         ++ack_index)
    {
        if (UwbParser_Consume(&ack_parser, bind_ack_stream[ack_index],
                              &ack_measurement) != 0u)
        {
            fputs("binding acknowledgement parsed as mc data\n", stderr);
            return 1;
        }
    }
    if (UwbParser_TakeBindAck(&ack_parser) == 0u ||
        UwbParser_TakeBindAck(&ack_parser) != 0u)
    {
        fputs("binding acknowledgement latch failed\n", stderr);
        return 1;
    }

    if (Replay(captured_stream, sizeof(captured_stream) - 1u,
               expected, sizeof(expected) / sizeof(expected[0])) != 0)
    {
        return 1;
    }
    if (Replay(extended_frame, sizeof(extended_frame) - 1u,
               extended_expected,
               sizeof(extended_expected) / sizeof(extended_expected[0])) != 0)
    {
        return 1;
    }

    puts("UwbParser regressions: PASS (bind ack, captured 6/6, extended 1/1)");
    return 0;
}
