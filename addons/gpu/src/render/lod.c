#include "backend/backend.h"
Uint32 sigpu_primitive_lod(sigpu_vec3_t center, float radius) {
    const RenderView *view = ecs_get_resource_read(RenderView);
    float depth = fmaxf(sigpu_mat4_transform_point(view->view, center).z, view->near_plane);
    float pixels = radius * view->lod_scale / depth;
    return pixels < SIGPU_LOD_LOW_MAX_PIXELS ? 0 : pixels < SIGPU_LOD_MEDIUM_MAX_PIXELS ? 1 : 2;
}
