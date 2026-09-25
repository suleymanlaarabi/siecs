#ifndef SIGPU_RENDER_INTERNAL_H
#define SIGPU_RENDER_INTERNAL_H
#include "backend/backend.h"
#include <siecs_spatial.h>
#include <sigpu.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define DECLARE_FIELD(T)                                                                           \
    typedef struct {                                                                               \
        const T *data;                                                                             \
        ptrdiff_t stride;                                                                          \
    } field_##T
DECLARE_FIELD(GlobalPosition3d);
DECLARE_FIELD(GlobalOrientation3d);
DECLARE_FIELD(GlobalScale3d);
DECLARE_FIELD(Color);
DECLARE_FIELD(Bloom);
#define FIELD(T, it, index)                                                                        \
    ((field_##T){ ecs_field(it, index), ecs_field_is_shared(it, index) ? 0 : 1 })
#define AT(f, index) ((f).data[(index) * (f).stride])
ECS_RELATION_DECLARE(RenderChunk);
ECS_COMPONENT_DECLARE(RenderChunkBounds, {
    int32_t cell_x, cell_y, cell_z;
    float center_x, center_y, center_z;
    float radius;
});
ECS_COMPONENT_DECLARE(RenderChunkGpuData, {
    uint32_t index;
    uint32_t axis_count[3];
    uint32_t rotated_count[3];
});
ECS_COMPONENT_DECLARE(RenderChunkVisibility, {
    uint8_t camera;
    uint8_t shadow;
});
ECS_TAG_DECLARE(RenderChunkDirty);
typedef struct {
    float x, y, z;
} primitive_size_t;
typedef struct {
    int16_t x, y, z, w;
} packed_rotation;
packed_rotation pack_rotation(GlobalOrientation3d q);
bool is_rotated(GlobalOrientation3d q);
primitive_size_t primitive_size(sigpu_primitive_t primitive, const void *component);
primitive_size_t scaled_size(primitive_size_t size, GlobalScale3d scale);
float primitive_radius(sigpu_primitive_t primitive, primitive_size_t size);
Uint32 primitive_mesh(sigpu_primitive_t primitive, Uint32 lod);
sigpu_shared_material_t make_shared_material(primitive_size_t size, Color color, float bloom);
sigpu_axis_instance_t make_owned_axis(GlobalPosition3d p, primitive_size_t s, Color c, float bloom);
sigpu_rotated_instance_t make_owned_rotated(
    GlobalPosition3d p,
    GlobalOrientation3d q,
    primitive_size_t s,
    Color c,
    float bloom
);
bool sigpu_visible_camera(GlobalPosition3d p, float radius, float aspect);
bool sigpu_visible_with_shadows(GlobalPosition3d p, float radius, float aspect);
void record_shared(
    sigpu_shared_batch_t **batches,
    Uint32 *count,
    Uint32 *capacity,
    Uint32 first,
    Uint32 end,
    sigpu_shared_material_t material,
    Uint32 mesh,
    Uint32 lod,
    bool rotated
);
void record_owned(
    sigpu_owned_batch_t **batches,
    Uint32 *count,
    Uint32 *capacity,
    Uint32 first,
    Uint32 end,
    Uint32 mesh
);
const void *
primitive_field(ecs_iter_t *it, sigpu_primitive_t primitive, uint32_t index, ptrdiff_t stride);
void sigpu_settings_register(void);
void sigpu_materials_init(void);
void sigpu_render_schedule_fini(void);
ecs_system_id_t sigpu_camera_register(void);
ecs_system_id_t sigpu_static_chunks_register(ecs_system_id_t camera_system);
void sigpu_static_chunks_invalidate(ecs_observer_event_t *event);
void sigpu_bounds_register(ecs_system_id_t static_cache_system);
ecs_system_id_t sigpu_primitives_register(void);
void sigpu_render_schedule_register(ecs_system_id_t input_system);
void sigpu_render_schedule_set_shadows(bool enabled);
void sigpu_render_schedule_set_bloom(bool enabled);
void sigpu_bounds_set_enabled(bool enabled);
void sigpu_bounds_reset(void);
void sigpu_primitives_set_shadows(bool enabled);
void sigpu_primitives_reset(void);
void sigpu_render_schedule_reset(void);
#endif
