#ifndef POSITION_SOLVER_H
#define POSITION_SOLVER_H

#include <stdint.h>

typedef struct
{
    float center_mm;
    float angle_deg;
} PositionResult;

/**
 * @brief Calculate the moving Anchor position from left and right Tag ranges.
 * @param left_mm Distance from the left Tag to the moving Anchor, in millimetres.
 * @param right_mm Distance from the right Tag to the moving Anchor, in millimetres.
 * @param baseline_mm Distance between the two Tag phase centres, in millimetres.
 * @param result Output center distance and signed angle; right is positive.
 * @return 1 for valid circle geometry, otherwise 0.
 */
uint8_t PositionSolver_Calculate(float left_mm,
                                 float right_mm,
                                 float baseline_mm,
                                 PositionResult *result);

#endif
