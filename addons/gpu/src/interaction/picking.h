#ifndef SIECS_TS_PICKING_H
#define SIECS_TS_PICKING_H

#include <stdbool.h>

typedef struct {
    float x, y, z;
} sipicking_vec3_t;

typedef struct {
    float x, y, z, w;
} sipicking_quat_t;

typedef struct {
    sipicking_vec3_t origin, direction;
} sipicking_ray_t;

typedef struct {
    float distance;
    sipicking_vec3_t point, normal;
} sipicking_hit_t;

bool sipicking_ray_obb(
    sipicking_ray_t ray,
    sipicking_vec3_t center,
    sipicking_quat_t orientation,
    sipicking_vec3_t half_extents,
    sipicking_hit_t *out
);

#endif
