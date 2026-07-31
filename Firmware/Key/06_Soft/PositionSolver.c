#include "PositionSolver.h"

#include <math.h>

#define POSITION_RAD_TO_DEG 57.2957795f

/** @brief Calculate center distance and signed bearing from two ranges. */
uint8_t PositionSolver_Calculate(float left_mm,
                                 float right_mm,
                                 float baseline_mm,
                                 PositionResult *result)
{
    float x;
    float y2;
    float y;

    if (result == 0 || left_mm <= 0.0f || right_mm <= 0.0f ||
        baseline_mm <= 0.0f)
    {
        return 0u;
    }
    if (fabsf(left_mm - right_mm) > baseline_mm ||
        (left_mm + right_mm) < baseline_mm)
    {
        return 0u;
    }

    x = ((left_mm * left_mm) - (right_mm * right_mm)) /
        (2.0f * baseline_mm);
    y2 = (left_mm * left_mm) -
         ((x + baseline_mm / 2.0f) * (x + baseline_mm / 2.0f));
    if (y2 < -1.0f) return 0u;
    if (y2 < 0.0f) y2 = 0.0f;

    y = sqrtf(y2);
    result->center_mm = sqrtf((x * x) + (y * y));
    result->angle_deg = atan2f(x, y) * POSITION_RAD_TO_DEG;
    return 1u;
}
