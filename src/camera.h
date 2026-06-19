#ifndef CHUCK_CAMERA_H
#define CHUCK_CAMERA_H

/* Position one viewport axis around a world-space focus point while keeping
 * both edges inside the world. Worlds smaller than the viewport stay fixed. */
float camera_axis_target(float focus, float world_size, float view_size);

#endif /* CHUCK_CAMERA_H */
