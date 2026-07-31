#ifndef LOCK_PACKET_H
#define LOCK_PACKET_H

#include <stdint.h>

#define LOCK_PACKET_MAGIC       0xD5u
#define LOCK_PACKET_VERSION     0x01u
#define LOCK_PACKET_FRAME_SIZE  12u
#define LOCK_DISTANCE_UNIT_MM   1u
#define LOCK_ANGLE_UNIT_DEG10   1u

typedef enum
{
    LOCK_PACKET_NONE = 0,
    LOCK_PACKET_POSITION = 1,
    LOCK_PACKET_DIAGNOSTIC = 2
} LockPacketKind;

typedef struct
{
    LockPacketKind kind;
    uint8_t key_id;
    uint8_t flags;
    uint16_t radial_distance_mm;
    int16_t angle_deg10;
    uint16_t sequence;
    uint8_t quality;
} LockPacket;

/**
 * @brief Decode a CC1101 application payload.
 * @param data Payload without CC1101 length/status bytes.
 * @param length Payload length.
 * @param packet Decoded packet output.
 * @return 1 for a valid position or diagnostic packet, otherwise 0.
 * @note Position frames use little-endian integer fields and CRC-8 over bytes 0..10:
 *       D5 01 ID FLAGS DIST_L DIST_H ANGLE_L ANGLE_H SEQ_L SEQ_H QUALITY CRC8.
 */
uint8_t LockPacket_Decode(const uint8_t *data, uint8_t length, LockPacket *packet);

/**
 * @brief Calculate the CRC-8 used by the application frame.
 * @param data Bytes to checksum.
 * @param length Number of bytes.
 * @return CRC-8 with polynomial 0x07 and initial value 0.
 */
uint8_t LockPacket_Crc8(const uint8_t *data, uint8_t length);

#endif
