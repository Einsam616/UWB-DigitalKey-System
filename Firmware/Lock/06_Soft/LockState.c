#include "LockState.h"

#define LOCK_FRONT_LIMIT_DEG10 450
#define LOCK_WELCOME_ENTER_MM  2000u
#define LOCK_WELCOME_EXIT_MM   2050u
#define LOCK_UNLOCK_ENTER_MM   1000u
#define LOCK_UNLOCK_EXIT_MM    1050u
#define LOCK_SENSING_ENTER_MM  3000u
#define LOCK_SENSING_EXIT_MM   3050u

/** @brief Apply identity, front-sector, welcome, and unlock decisions. */
LockState LockState_Classify(uint16_t distance_mm,
                             int16_t angle_deg10,
                             uint8_t id_valid,
                             LockState previous)
{
    int16_t angle = angle_deg10;
    if (id_valid == 0u) return LOCK_STATE_INVALID;
    if (angle > LOCK_FRONT_LIMIT_DEG10 || angle < -LOCK_FRONT_LIMIT_DEG10)
    {
        return LOCK_STATE_OUTSIDE;
    }

    if (previous == LOCK_STATE_UNLOCKED && distance_mm <= LOCK_UNLOCK_EXIT_MM)
    {
        return LOCK_STATE_UNLOCKED;
    }
    if (distance_mm <= LOCK_UNLOCK_ENTER_MM) return LOCK_STATE_UNLOCKED;
    if ((previous == LOCK_STATE_WELCOME || previous == LOCK_STATE_UNLOCKED) &&
        distance_mm <= LOCK_WELCOME_EXIT_MM)
    {
        return LOCK_STATE_WELCOME;
    }
    if (distance_mm <= LOCK_WELCOME_ENTER_MM) return LOCK_STATE_WELCOME;
    if ((previous == LOCK_STATE_SENSING || previous == LOCK_STATE_WELCOME) &&
        distance_mm <= LOCK_SENSING_EXIT_MM)
    {
        return LOCK_STATE_SENSING;
    }
    if (distance_mm <= LOCK_SENSING_ENTER_MM) return LOCK_STATE_SENSING;
    return LOCK_STATE_LOCKED;
}

/** @brief Map a state enum to a fixed display string. */
const char *LockState_Name(LockState state)
{
    switch (state)
    {
        case LOCK_STATE_SENSING: return "SENSING";
        case LOCK_STATE_WELCOME: return "WELCOME";
        case LOCK_STATE_UNLOCKED: return "UNLOCKED";
        case LOCK_STATE_LOST: return "LOST";
        case LOCK_STATE_INVALID: return "INVALID";
        case LOCK_STATE_OUTSIDE: return "OUTSIDE";
        default: return "LOCKED";
    }
}
