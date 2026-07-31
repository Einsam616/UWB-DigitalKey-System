#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "PositionSolver.h"

static int CheckPoint(float x_mm, float y_mm, float expected_angle_deg)
{
    const float baseline_mm = 500.0f;
    float left_mm = sqrtf(((x_mm + baseline_mm / 2.0f) *
                           (x_mm + baseline_mm / 2.0f)) + (y_mm * y_mm));
    float right_mm = sqrtf(((x_mm - baseline_mm / 2.0f) *
                            (x_mm - baseline_mm / 2.0f)) + (y_mm * y_mm));
    float expected_center_mm = sqrtf((x_mm * x_mm) + (y_mm * y_mm));
    PositionResult result;

    if (PositionSolver_Calculate(left_mm, right_mm, baseline_mm, &result) == 0u)
    {
        fprintf(stderr, "valid point rejected\n");
        return 1;
    }
    if (fabsf(result.center_mm - expected_center_mm) > 0.5f ||
        fabsf(result.angle_deg - expected_angle_deg) > 0.1f)
    {
        fprintf(stderr, "got D=%.2f A=%.2f, expected D=%.2f A=%.2f\n",
                result.center_mm, result.angle_deg,
                expected_center_mm, expected_angle_deg);
        return 1;
    }
    return 0;
}

int main(void)
{
    PositionResult result;

    if (CheckPoint(0.0f, 1300.0f, 0.0f) != 0) return 1;
    if (CheckPoint(750.0f, 1299.0381f, 30.0f) != 0) return 1;
    if (CheckPoint(-750.0f, 1299.0381f, -30.0f) != 0) return 1;
    if (PositionSolver_Calculate(100.0f, 1000.0f, 500.0f, &result) != 0u)
    {
        fprintf(stderr, "invalid triangle accepted\n");
        return 1;
    }

    puts("PositionSolver regression: PASS (0deg, +30deg, -30deg, invalid)");
    return 0;
}
