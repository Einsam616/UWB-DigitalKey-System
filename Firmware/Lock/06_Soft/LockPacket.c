#include "LockPacket.h"

/** @brief Calculate CRC-8 for a short static application frame. */
uint8_t LockPacket_Crc8(const uint8_t *data, uint8_t length)
{
    uint8_t crc = 0u;
    uint8_t index;
    uint8_t bit;

    if (data == 0) return 0u;
    for (index = 0u; index < length; ++index)
    {
        crc = (uint8_t)(crc ^ data[index]);
        for (bit = 0u; bit < 8u; ++bit)
        {
            crc = (crc & 0x80u) ? (uint8_t)((crc << 1u) ^ 0x07u)
                                  : (uint8_t)(crc << 1u);
        }
    }
    return crc;
}

/** @brief Decode a formal position frame or the existing ASCII 666 radio test. */
uint8_t LockPacket_Decode(const uint8_t *data, uint8_t length, LockPacket *packet)
{
    uint16_t raw_angle;

    if (data == 0 || packet == 0) return 0u;
    packet->kind = LOCK_PACKET_NONE;
    packet->key_id = 0u;
    packet->flags = 0u;
    packet->radial_distance_mm = 0u;
    packet->angle_deg10 = 0;
    packet->sequence = 0u;
    packet->quality = 0u;

    if (length == 3u && data[0] == (uint8_t)'6' &&
        data[1] == (uint8_t)'6' && data[2] == (uint8_t)'6')
    {
        packet->kind = LOCK_PACKET_DIAGNOSTIC;
        return 1u;
    }

    if (length != LOCK_PACKET_FRAME_SIZE || data[0] != LOCK_PACKET_MAGIC ||
        data[1] != LOCK_PACKET_VERSION ||
        LockPacket_Crc8(data, (uint8_t)(LOCK_PACKET_FRAME_SIZE - 1u)) != data[11])
    {
        return 0u;
    }

    packet->kind = LOCK_PACKET_POSITION;
    packet->key_id = (uint8_t)(data[2] & 0x0Fu);
    packet->flags = data[3];
    packet->radial_distance_mm = (uint16_t)data[4] | (uint16_t)((uint16_t)data[5] << 8u);
    raw_angle = (uint16_t)data[6] | (uint16_t)((uint16_t)data[7] << 8u);
    packet->angle_deg10 = (int16_t)raw_angle;
    packet->sequence = (uint16_t)data[8] | (uint16_t)((uint16_t)data[9] << 8u);
    packet->quality = data[10];
    return 1u;
}
