#include "render/render_internal.h"
typedef struct {
    union {
        sigpu_shared_axis_instance_t shared_axis;
        sigpu_shared_rotated_instance_t shared_rotated;
        sigpu_axis_instance_t owned_axis;
        sigpu_rotated_instance_t owned_rotated;
    } instance;
    Uint32 next;
} primitive_candidate_t;

static void append_candidate(
    primitive_candidate_t *candidates,
    Uint32 heads[6],
    Uint32 tails[6],
    Uint32 *count,
    Uint32 bucket,
    primitive_candidate_t candidate
) {
    Uint32 index = (*count)++;
    candidate.next = UINT32_MAX;
    candidates[index] = candidate;
    if (tails[bucket] != UINT32_MAX)
        candidates[tails[bucket]].next = index;
    else
        heads[bucket] = index;
    tails[bucket] = index;
}

static void render_primitives(ecs_iter_t *it, sigpu_primitive_t primitive) {
    if (!SIGPU_FRAMECONTEXT->frame_width || !SIGPU_FRAMECONTEXT->frame_height)
        return;
    Uint64 start = SIGPU_RENDERSTATS->profile_enabled ? SDL_GetTicksNS() : 0;
    field_GlobalPosition3d positions = FIELD(GlobalPosition3d, it, 0);
    field_GlobalOrientation3d rotations = FIELD(GlobalOrientation3d, it, 1);
    field_GlobalScale3d scales = FIELD(GlobalScale3d, it, 2);
    field_Color colors = FIELD(Color, it, 4);
    field_Bloom blooms = FIELD(Bloom, it, 5);
    ptrdiff_t shape_stride = ecs_field_is_shared(it, 3) ? 0 : 1;
    bool shared =
        !shape_stride && ecs_field_is_shared(it, 4) && (!blooms.data || ecs_field_is_shared(it, 5));
    primitive_size_t shared_size = { 0 };
    sigpu_shared_material_t material = { 0 };
    if (shared) {
        shared_size = primitive_size(primitive, primitive_field(it, primitive, 0, 0));
        float bloom = blooms.data ? fmaxf(AT(blooms, 0).intensity, 0.0f) : 0.0f;
        material = make_shared_material(shared_size, AT(colors, 0), bloom);
    }
    primitive_candidate_t *candidates = SDL_malloc(it->count * sizeof(*candidates));
    Uint32 heads[6], tails[6], count = 0;
    for (Uint32 i = 0; i < 6; i++)
        heads[i] = tails[i] = UINT32_MAX;
    float aspect = (float)SIGPU_FRAMECONTEXT->frame_width / SIGPU_FRAMECONTEXT->frame_height;
    bool (*visible_fn)(GlobalPosition3d, float, float) =
        SIGPU_RENDERSETTINGS->shadows_enabled ? sigpu_visible_with_shadows : sigpu_visible_camera;
    for (Uint32 i = 0; i < it->count; i++) {
        GlobalPosition3d position = AT(positions, i);
        GlobalScale3d scale = AT(scales, i);
        primitive_size_t size = scaled_size(
            shared ? shared_size
                   : primitive_size(primitive, primitive_field(it, primitive, i, shape_stride)),
            scale
        );
        float radius = primitive_radius(primitive, size);
        if (!visible_fn(position, radius, aspect))
            continue;
        Uint32 lod =
            primitive == SIGPU_PRIMITIVE_CUBE
                ? 0
                : sigpu_primitive_lod((sigpu_vec3_t){ position.x, position.y, position.z }, radius);
        GlobalOrientation3d rotation = AT(rotations, i);
        bool rotated = is_rotated(rotation);
        Uint32 bucket = lod * 2 + rotated;
        primitive_candidate_t candidate = { 0 };
        if (shared) {
            if (rotated) {
                packed_rotation packed = pack_rotation(rotation);
                candidate.instance.shared_rotated =
                    (sigpu_shared_rotated_instance_t){ position.x, position.y, position.z, scale.x,
                                                       scale.y,    scale.z,    packed.x,   packed.y,
                                                       packed.z,   packed.w };
            } else {
                candidate.instance.shared_axis =
                    (sigpu_shared_axis_instance_t){ position.x, position.y, position.z,
                                                    scale.x,    scale.y,    scale.z };
            }
            SIGPU_RENDERQUEUE->any_bloom |= material.size_bloom[3] > 0.0f;
        } else {
            float bloom = blooms.data ? fmaxf(AT(blooms, i).intensity, 0.0f) : 0.0f;
            Color color = AT(colors, i);
            if (rotated)
                candidate.instance.owned_rotated =
                    make_owned_rotated(position, rotation, size, color, bloom);
            else
                candidate.instance.owned_axis = make_owned_axis(position, size, color, bloom);
            SIGPU_RENDERQUEUE->any_bloom |= bloom > 0.0f;
        }
        append_candidate(candidates, heads, tails, &count, bucket, candidate);
    }
    sigpu_instances_reserve(count);
    Uint32 max_lod = primitive == SIGPU_PRIMITIVE_CUBE ? 0 : 2;
    for (Uint32 lod = 0; lod <= max_lod; lod++) {
        for (Uint32 rotated = 0; rotated < 2; rotated++) {
            Uint32 bucket = lod * 2 + rotated;
            Uint32 first = shared ? (rotated ? SIGPU_RENDERQUEUE->shared_rotated_count
                                             : SIGPU_RENDERQUEUE->shared_axis_count)
                                  : (rotated ? SIGPU_RENDERQUEUE->owned_rotated_count
                                             : SIGPU_RENDERQUEUE->owned_axis_count);
            for (Uint32 i = heads[bucket]; i != UINT32_MAX; i = candidates[i].next) {
                primitive_candidate_t *candidate = &candidates[i];
                if (shared) {
                    if (rotated)
                        ((sigpu_shared_rotated_instance_t *)SIGPU_RENDERQUEUE
                             ->rotated_mapped)[SIGPU_RENDERQUEUE->shared_rotated_count++] =
                            candidate->instance.shared_rotated;
                    else
                        ((
                             sigpu_shared_axis_instance_t *
                        )SIGPU_RENDERQUEUE->axis_mapped)[SIGPU_RENDERQUEUE->shared_axis_count++] =
                            candidate->instance.shared_axis;
                } else {
                    if (rotated)
                        ((sigpu_rotated_instance_t *)SIGPU_RENDERQUEUE->rotated_mapped)
                            [SIGPU_GPUCONTEXT->rotated_capacity -
                             ++SIGPU_RENDERQUEUE->owned_rotated_count] =
                                candidate->instance.owned_rotated;
                    else
                        ((sigpu_axis_instance_t *)SIGPU_RENDERQUEUE->axis_mapped)
                            [SIGPU_GPUCONTEXT->axis_capacity -
                             ++SIGPU_RENDERQUEUE->owned_axis_count] =
                                candidate->instance.owned_axis;
                }
            }
            Uint32 end = shared ? (rotated ? SIGPU_RENDERQUEUE->shared_rotated_count
                                           : SIGPU_RENDERQUEUE->shared_axis_count)
                                : (rotated ? SIGPU_RENDERQUEUE->owned_rotated_count
                                           : SIGPU_RENDERQUEUE->owned_axis_count);
            Uint32 mesh = primitive_mesh(primitive, lod);
            if (shared) {
                record_shared(
                    rotated ? &SIGPU_RENDERQUEUE->shared_rotated_batches
                            : &SIGPU_RENDERQUEUE->shared_axis_batches,
                    rotated ? &SIGPU_RENDERQUEUE->shared_rotated_batch_count
                            : &SIGPU_RENDERQUEUE->shared_axis_batch_count,
                    rotated ? &SIGPU_RENDERQUEUE->shared_rotated_batch_capacity
                            : &SIGPU_RENDERQUEUE->shared_axis_batch_capacity,
                    first,
                    end,
                    material,
                    mesh,
                    lod,
                    rotated
                );
            } else {
                record_owned(
                    rotated ? &SIGPU_RENDERQUEUE->owned_rotated_batches
                            : &SIGPU_RENDERQUEUE->owned_axis_batches,
                    rotated ? &SIGPU_RENDERQUEUE->owned_rotated_batch_count
                            : &SIGPU_RENDERQUEUE->owned_axis_batch_count,
                    rotated ? &SIGPU_RENDERQUEUE->owned_rotated_batch_capacity
                            : &SIGPU_RENDERQUEUE->owned_axis_batch_capacity,
                    first,
                    end,
                    mesh
                );
            }
        }
    }
    SDL_free(candidates);
    if (SIGPU_RENDERSTATS->profile_enabled)
        SIGPU_RENDERSTATS->collect_ns += SDL_GetTicksNS() - start;
}
static void render_cuboids(ecs_iter_t *it) { render_primitives(it, SIGPU_PRIMITIVE_CUBE); }
static void render_cylinders(ecs_iter_t *it) { render_primitives(it, SIGPU_PRIMITIVE_CYLINDER); }
static void render_spheres(ecs_iter_t *it) { render_primitives(it, SIGPU_PRIMITIVE_SPHERE); }
static void cull_static_primitives(ecs_iter_t *it) {
    Uint64 start = SIGPU_RENDERSTATS->profile_enabled ? SDL_GetTicksNS() : 0;
    if (!SIGPU_FRAMECONTEXT->frame_width || !SIGPU_FRAMECONTEXT->frame_height)
        return;
    sigpu_static_cull((float)SIGPU_FRAMECONTEXT->frame_width / SIGPU_FRAMECONTEXT->frame_height);
    for (Uint32 i = 0; i < SIGPU_STATICRENDERCACHE->static_chunk_count; i++) {
        const sigpu_static_chunk_t *chunk = &SIGPU_STATICRENDERCACHE->static_chunks[i];
        RenderChunkVisibility *visibility =
            ecs_get(SIGPU_STATICRENDERCACHE->chunk_info[i].entity, RenderChunkVisibility);
        visibility->camera = chunk->camera_visible;
        visibility->shadow = chunk->shadow_visible;
    }
    if (SIGPU_RENDERSTATS->profile_enabled)
        SIGPU_RENDERSTATS->cull_ns += SDL_GetTicksNS() - start;
}
static ecs_system_id_t shadow_cull_system;

static void cull_static_shadows(ecs_iter_t *it) {
    if (!SIGPU_FRAMECONTEXT->swapchain)
        return;
    sigpu_static_shadow_cull();
    for (Uint32 i = 0; i < SIGPU_STATICRENDERCACHE->static_chunk_count; i++) {
        const sigpu_static_chunk_t *chunk = &SIGPU_STATICRENDERCACHE->static_chunks[i];
        ecs_get(SIGPU_STATICRENDERCACHE->chunk_info[i].entity, RenderChunkVisibility)->shadow =
            chunk->shadow_visible;
    }
}

void sigpu_primitives_set_shadows(bool enabled) {
    if (!shadow_cull_system)
        return;
    if (enabled)
        ecs_system_enable(shadow_cull_system);
    else
        ecs_system_disable(shadow_cull_system);
}

void sigpu_primitives_reset(void) { shadow_cull_system = 0; }

ecs_system_id_t sigpu_primitives_register(void) {
    ecs_system_desc_t cull = { .name = "CullStatic",
                               .callback = cull_static_primitives,
                               .phase = EcsOnRender,
                               .main_thread_only = true };
    ecs_system_id_t cull_system = ecs_system_init(&cull);
    shadow_cull_system = ecs_system(
        {
            .name = "ShadowCull",
            .phase = EcsOnRender,
            .after = { cull_system },
            .callback = cull_static_shadows,
            .main_thread_only = true,
        }
    );
    ecs_system_id_t previous = shadow_cull_system;
    uint16_t shapes[SIGPU_PRIMITIVE_COUNT] = { ecs_id(Cuboid), ecs_id(Cylinder), ecs_id(Sphere) };
    void (*callbacks[SIGPU_PRIMITIVE_COUNT])(ecs_iter_t *) = { render_cuboids,
                                                               render_cylinders,
                                                               render_spheres };
    const char *names[SIGPU_PRIMITIVE_COUNT] = { "ExtractCuboids",
                                                 "ExtractCylinders",
                                                 "ExtractSpheres" };
    for (Uint32 p = 0; p < SIGPU_PRIMITIVE_COUNT; p++) {
        ecs_system_desc_t system={.name=names[p],.query={.components={
            {.id=ecs_id(GlobalPosition3d),.access=EcsIn},
            {.id=ecs_id(GlobalOrientation3d),.access=EcsIn},
            {.id=ecs_id(GlobalScale3d),.access=EcsIn},
            {.id=shapes[p],.access=EcsIn},
            {.id=ecs_id(Color),.access=EcsIn},
            {.id=ecs_id(Bloom),.access=EcsInOptional},
            {.id=ecs_id(Static3dReady),.access=EcsNot},
        }},.callback=callbacks[p],.phase=EcsOnRender,.after={previous},.main_thread_only=true};
        previous = ecs_system_init(&system);
    }
    return previous;
}
