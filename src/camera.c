#include "camera.h"

float camera_axis_target(float focus, float world_size, float view_size)
{
    if (view_size <= 0.0f || world_size <= view_size)
        return 0.0f;

    float target = focus - view_size * 0.5f;
    float maximum = world_size - view_size;
    if (target < 0.0f)
        return 0.0f;
    if (target > maximum)
        return maximum;
    return target;
}
