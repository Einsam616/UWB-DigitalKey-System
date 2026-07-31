#ifndef LOCK_STATE_H
#define LOCK_STATE_H

#include <stdint.h>

typedef enum
{
    LOCK_STATE_LOCKED = 0,
    LOCK_STATE_SENSING,
    LOCK_STATE_WELCOME,
    LOCK_STATE_UNLOCKED,
    LOCK_STATE_LOST,
    LOCK_STATE_INVALID,
    LOCK_STATE_OUTSIDE
} LockState;

/**
 * @brief Classify a valid key position and apply the C题 zone hysteresis.
 * @param distance_mm Radial distance from the 60 cm cylinder boundary.
 * @param angle_deg10 Signed angle in tenths of a degree.
 * @param id_valid Whether the received four-bit ID matches the lock setting.
 * @param previous Previous state, used for 50 mm boundary hysteresis.
 * @return New state. The radial zones are 0-1 m unlock, 1-2 m welcome,
 *         2-3 m sensing, and beyond 3 m outside the functional area.
 */
LockState LockState_Classify(uint16_t distance_mm,
                             int16_t angle_deg10,
                             uint8_t id_valid,
                             LockState previous);

/**
 * @brief Return a display-safe uppercase state name.
 * @param state State value.
 * @return Static ASCII state string.
 */
const char *LockState_Name(LockState state);

#endif
