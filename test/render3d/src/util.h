#ifndef RENDER3D_TEST_UTIL_H
#define RENDER3D_TEST_UTIL_H
#include "render3d_internal.h"
#include "sigpu_internal.h"
#include <bake_test.h>

/* A world on the offscreen video driver with a window "main", a device, a
 * surface, the sun overhead and a camera at (0, 0, 10) looking at -z. */
typedef struct {
    ecs_entity_t window, surface, camera;
} r3d_test_t;

r3d_test_t r3d_test_start(int32_t width, int32_t height, int32_t samples);
void r3d_test_stop(void);
void r3d_test_frames(uint32_t count);
/* Pixel of the surface's color target, as 0xAABBGGRR (sRGB encoded). */
uint32_t r3d_test_pixel(const r3d_test_t *t, float u, float v);
uint8_t r3d_channel(uint32_t pixel, uint32_t channel);
const GpuStats *r3d_test_stats(void);
#endif
