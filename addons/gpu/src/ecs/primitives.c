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

static void emit_candidates(
    primitive_candidate_t *candidates, Uint32 heads[6], Uint32 count,
    sigpu_primitive_t primitive, bool shared, sigpu_shared_material_t material, bool shadow
) {
    if (!count)
        return;
    if (shadow)
        sigpu_shadow_instances_reserve(count);
    else
        sigpu_instances_reserve(count);
    RenderQueue *queue = SIGPU_RENDERQUEUE;
    Uint32 *shared_counts[2] = {
        shadow ? &queue->shadow_shared_axis_count : &queue->shared_axis_count,
        shadow ? &queue->shadow_shared_rotated_count : &queue->shared_rotated_count,
    };
    Uint32 *owned_counts[2] = {
        shadow ? &queue->shadow_owned_axis_count : &queue->owned_axis_count,
        shadow ? &queue->shadow_owned_rotated_count : &queue->owned_rotated_count,
    };
    void *mapped[2] = {
        shadow ? queue->shadow_axis_mapped : queue->axis_mapped,
        shadow ? queue->shadow_rotated_mapped : queue->rotated_mapped,
    };
    Uint32 capacities[2] = {
        shadow ? SIGPU_GPUCONTEXT->shadow_axis_capacity : SIGPU_GPUCONTEXT->axis_capacity,
        shadow ? SIGPU_GPUCONTEXT->shadow_rotated_capacity : SIGPU_GPUCONTEXT->rotated_capacity,
    };
    sigpu_shared_batch_t **shared_batches[2] = {
        shadow ? &queue->shadow_shared_axis_batches : &queue->shared_axis_batches,
        shadow ? &queue->shadow_shared_rotated_batches : &queue->shared_rotated_batches,
    };
    Uint32 *shared_batch_counts[2] = {
        shadow ? &queue->shadow_shared_axis_batch_count : &queue->shared_axis_batch_count,
        shadow ? &queue->shadow_shared_rotated_batch_count : &queue->shared_rotated_batch_count,
    };
    Uint32 *shared_batch_capacities[2] = {
        shadow ? &queue->shadow_shared_axis_batch_capacity : &queue->shared_axis_batch_capacity,
        shadow ? &queue->shadow_shared_rotated_batch_capacity : &queue->shared_rotated_batch_capacity,
    };
    sigpu_owned_batch_t **owned_batches[2] = {
        shadow ? &queue->shadow_owned_axis_batches : &queue->owned_axis_batches,
        shadow ? &queue->shadow_owned_rotated_batches : &queue->owned_rotated_batches,
    };
    Uint32 *owned_batch_counts[2] = {
        shadow ? &queue->shadow_owned_axis_batch_count : &queue->owned_axis_batch_count,
        shadow ? &queue->shadow_owned_rotated_batch_count : &queue->owned_rotated_batch_count,
    };
    Uint32 *owned_batch_capacities[2] = {
        shadow ? &queue->shadow_owned_axis_batch_capacity : &queue->owned_axis_batch_capacity,
        shadow ? &queue->shadow_owned_rotated_batch_capacity : &queue->owned_rotated_batch_capacity,
    };
    Uint32 max_lod = primitive == SIGPU_PRIMITIVE_CUBE ? 0 : 2;
    for (Uint32 lod = 0; lod <= max_lod; lod++) {
        for (Uint32 rotated = 0; rotated < 2; rotated++) {
            Uint32 bucket = lod * 2 + rotated;
            Uint32 first = shared ? *shared_counts[rotated] : *owned_counts[rotated];
            for (Uint32 i = heads[bucket]; i != UINT32_MAX; i = candidates[i].next) {
                primitive_candidate_t *candidate = &candidates[i];
                if (shared) {
                    if (rotated)
                        ((sigpu_shared_rotated_instance_t *)mapped[1])[(*shared_counts[1])++] = candidate->instance.shared_rotated;
                    else
                        ((sigpu_shared_axis_instance_t *)mapped[0])[(*shared_counts[0])++] = candidate->instance.shared_axis;
                } else {
                    if (rotated)
                        ((sigpu_rotated_instance_t *)mapped[1])[capacities[1] - ++*owned_counts[1]] = candidate->instance.owned_rotated;
                    else
                        ((sigpu_axis_instance_t *)mapped[0])[capacities[0] - ++*owned_counts[0]] = candidate->instance.owned_axis;
                }
            }
            Uint32 end = shared ? *shared_counts[rotated] : *owned_counts[rotated];
            Uint32 mesh = primitive_mesh(primitive, lod);
            if (shared)
                record_shared(shared_batches[rotated], shared_batch_counts[rotated],
                    shared_batch_capacities[rotated], first, end, material, mesh, lod, rotated);
            else
                record_owned(owned_batches[rotated], owned_batch_counts[rotated],
                    owned_batch_capacities[rotated], first, end, mesh);
        }
    }
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
    bool shared = !shape_stride && ecs_field_is_shared(it, 4) &&
        (!blooms.data || ecs_field_is_shared(it, 5));
    primitive_size_t shared_size = { 0 };
    sigpu_shared_material_t material = { 0 };
    if (shared) {
        shared_size = primitive_size(primitive, primitive_field(it, primitive, 0, 0));
        float bloom = blooms.data ? fmaxf(AT(blooms, 0).intensity, 0.0f) : 0.0f;
        material = make_shared_material(shared_size, AT(colors, 0), bloom);
    }
    primitive_candidate_t *candidates[2] = {
        SDL_malloc(it->count * sizeof(*candidates[0])),
        SDL_malloc(it->count * sizeof(*candidates[1])),
    };
    Uint32 heads[2][6], tails[2][6], counts[2] = { 0, 0 };
    for (Uint32 q = 0; q < 2; q++)
        for (Uint32 bucket = 0; bucket < 6; bucket++)
            heads[q][bucket] = tails[q][bucket] = UINT32_MAX;
    float aspect = (float)SIGPU_FRAMECONTEXT->frame_width / SIGPU_FRAMECONTEXT->frame_height;
    for (Uint32 i = 0; i < it->count; i++) {
        GlobalPosition3d position = AT(positions, i);
        GlobalScale3d scale = AT(scales, i);
        primitive_size_t size = scaled_size(
            shared ? shared_size : primitive_size(primitive, primitive_field(it, primitive, i, shape_stride)),
            scale
        );
        float radius = primitive_radius(primitive, size);
        sigpu_vec3_t center = { position.x, position.y, position.z };
        bool camera_visible = sigpu_camera_visible(center, radius, aspect);
        bool shadow_visible = SIGPU_RENDERSETTINGS->shadows_enabled && sigpu_shadow_visible(center, radius);
        if (!camera_visible && !shadow_visible)
            continue;
        Uint32 lod = primitive == SIGPU_PRIMITIVE_CUBE ? 0 : sigpu_primitive_lod(center, radius);
        GlobalOrientation3d rotation = AT(rotations, i);
        bool rotated = is_rotated(rotation);
        Uint32 bucket = lod * 2 + rotated;
        primitive_candidate_t candidate = { 0 };
        if (shared) {
            if (rotated) {
                packed_rotation packed = pack_rotation(rotation);
                candidate.instance.shared_rotated = (sigpu_shared_rotated_instance_t){
                    position.x, position.y, position.z, scale.x, scale.y, scale.z,
                    packed.x, packed.y, packed.z, packed.w
                };
            } else {
                candidate.instance.shared_axis = (sigpu_shared_axis_instance_t){
                    position.x, position.y, position.z, scale.x, scale.y, scale.z
                };
            }
            if (camera_visible)
                SIGPU_RENDERQUEUE->any_bloom |= material.size_bloom[3] > 0.0f;
        } else {
            float bloom = blooms.data ? fmaxf(AT(blooms, i).intensity, 0.0f) : 0.0f;
            Color color = AT(colors, i);
            if (rotated)
                candidate.instance.owned_rotated = make_owned_rotated(position, rotation, size, color, bloom);
            else
                candidate.instance.owned_axis = make_owned_axis(position, size, color, bloom);
            if (camera_visible)
                SIGPU_RENDERQUEUE->any_bloom |= bloom > 0.0f;
        }
        if (camera_visible)
            append_candidate(candidates[0], heads[0], tails[0], &counts[0], bucket, candidate);
        if (shadow_visible)
            append_candidate(candidates[1], heads[1], tails[1], &counts[1], bucket, candidate);
    }
    emit_candidates(candidates[0], heads[0], counts[0], primitive, shared, material, false);
    emit_candidates(candidates[1], heads[1], counts[1], primitive, shared, material, true);
    SDL_free(candidates[0]);
    SDL_free(candidates[1]);
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
