#include "render/render_internal.h"

ECS_RELATION_DEFINE(
    RenderChunk,
    {
        .storage = EcsRelationByTarget,
        .on_delete_target = EcsRemoveRelation,
    }
);
ECS_COMPONENT_DEFINE(RenderChunkBounds);
ECS_COMPONENT_DEFINE(RenderChunkGpuData);
ECS_COMPONENT_DEFINE(RenderChunkVisibility);
ECS_TAG_DEFINE(RenderChunkDirty);
static ecs_query_id_t static_ready_query;

static bool affects_static(ecs_component_t component) {
    return component == ecs_id(Static3dReady) || component == ecs_id(Position3d) ||
           component == ecs_id(Rotation3d) || component == ecs_id(Scale3d) ||
           component == ecs_id(Cuboid) || component == ecs_id(Cylinder) ||
           component == ecs_id(Sphere) || component == ecs_id(Color) || component == ecs_id(Bloom);
}

static void enqueue_entity(ecs_entity_t entity) {
    StaticRenderCache *cache = SIGPU_STATICRENDERCACHE;
    if (cache->dirty_count == cache->dirty_capacity) {
        cache->dirty_capacity = cache->dirty_capacity ? cache->dirty_capacity * 2 : 64;
        cache->dirty_entities = SDL_realloc(
            cache->dirty_entities,
            cache->dirty_capacity * sizeof(*cache->dirty_entities)
        );
    }
    cache->dirty_entities[cache->dirty_count++] = entity;
}

static void enqueue_root(ecs_entity_t entity, bool prototype) {
    StaticRenderCache *cache = SIGPU_STATICRENDERCACHE;
    if (cache->dirty_root_count == cache->dirty_root_capacity) {
        cache->dirty_root_capacity =
            cache->dirty_root_capacity ? cache->dirty_root_capacity * 2 : 8;
        cache->dirty_roots = SDL_realloc(
            cache->dirty_roots,
            cache->dirty_root_capacity * sizeof(*cache->dirty_roots)
        );
    }
    cache->dirty_roots[cache->dirty_root_count++] = (sigpu_dirty_root_t){ entity, prototype };
}

static void mark_chunk_dirty(ecs_entity_t entity) {
    if (entity && ecs_is_alive(entity))
        ecs_add(entity, RenderChunkDirty);
}

void sigpu_static_chunks_invalidate(ecs_observer_event_t *event) {
    if (event->event == EcsOnRelationSet || event->event == EcsOnRelationRemove) {
        const ecs_relation_event_t *relation = event->trigger_data;
        if (!relation)
            return;
        if (relation->relation == ecs_rid(RenderChunk)) {
            mark_chunk_dirty(relation->old_target);
            mark_chunk_dirty(relation->new_target);
            return;
        }
        if (!ecs_has(event->entity, Static3dReady) && !ecs_target(event->entity, RenderChunk))
            return;
        if (relation->relation == ecs_rid(ChildOf)) {
            enqueue_root(event->entity, false);
        } else if (relation->relation == ecs_rid(IsA)) {
            enqueue_root(event->entity, true);
        } else
            return;
    } else if (!affects_static(event->component)) {
        return;
    }
    ecs_entity_t old_chunk = ecs_target(event->entity, RenderChunk);
    if (!old_chunk && event->component != ecs_id(Static3dReady) &&
        !ecs_has(event->entity, Static3dReady)) {
        if (ecs_has(event->entity, Abstract))
            enqueue_root(event->entity, true);
        return;
    }
    mark_chunk_dirty(old_chunk);
    if (ecs_is_alive(event->entity))
        enqueue_entity(event->entity);
}

static ecs_entity_t find_or_create_chunk(int32_t x, int32_t y, int32_t z) {
    StaticRenderCache *cache = SIGPU_STATICRENDERCACHE;
    for (Uint32 i = 0; i < cache->static_chunk_count; i++) {
        sigpu_chunk_info_t *chunk = &cache->chunk_info[i];
        if (chunk->cell_x == x && chunk->cell_y == y && chunk->cell_z == z)
            return chunk->entity;
    }
    if (cache->static_chunk_count == cache->static_chunk_capacity) {
        cache->static_chunk_capacity =
            cache->static_chunk_capacity ? cache->static_chunk_capacity * 2 : 16;
        cache->static_chunks = SDL_realloc(
            cache->static_chunks,
            cache->static_chunk_capacity * sizeof(*cache->static_chunks)
        );
        cache->chunk_info = SDL_realloc(
            cache->chunk_info,
            cache->static_chunk_capacity * sizeof(*cache->chunk_info)
        );
    }
    Uint32 index = cache->static_chunk_count++;
    ecs_entity_t entity = ecs_new();
    cache->static_chunks[index] = (sigpu_static_chunk_t){ 0 };
    cache->chunk_info[index] = (sigpu_chunk_info_t){ entity, x, y, z };
    ecs_set(entity, RenderChunkBounds, { .cell_x = x, .cell_y = y, .cell_z = z });
    ecs_set(entity, RenderChunkGpuData, { .index = index });
    ecs_set(entity, RenderChunkVisibility, { 0 });
    ecs_add(entity, RenderChunkDirty);
    return entity;
}

static bool child_of_root(ecs_entity_t entity, ecs_entity_t root) {
    for (ecs_entity_t parent = ecs_target(entity, ChildOf); parent;
         parent = ecs_target(parent, ChildOf))
        if (parent == root)
            return true;
    return false;
}

static void expand_dirty_roots(void) {
    StaticRenderCache *cache = SIGPU_STATICRENDERCACHE;
    if (!cache->dirty_root_count)
        return;
    for (ecs_iter_t ready = ecs_query_iter(static_ready_query); ecs_iter_next(&ready);) {
        for (Uint32 i = 0; i < ready.count; i++) {
            ecs_entity_t entity = ready.entities[i];
            for (Uint32 j = 0; j < cache->dirty_root_count; j++) {
                sigpu_dirty_root_t root = cache->dirty_roots[j];
                if (!ecs_is_alive(root.entity))
                    continue;
                if (root.prototype ? ecs_is(entity, root.entity)
                                   : child_of_root(entity, root.entity)) {
                    mark_chunk_dirty(ecs_target(entity, RenderChunk));
                    enqueue_entity(entity);
                    break;
                }
            }
        }
    }
    cache->dirty_root_count = 0;
}

static void assign_dirty_static_objects(ecs_iter_t *it) {
    StaticRenderCache *cache = SIGPU_STATICRENDERCACHE;
    expand_dirty_roots();
    Uint32 count = cache->dirty_count;
    for (Uint32 i = 0; i < count; i++) {
        ecs_entity_t entity = cache->dirty_entities[i];
        if (!ecs_is_alive(entity))
            continue;
        ecs_entity_t old_chunk = ecs_target(entity, RenderChunk);
        if (!ecs_has(entity, Static3dReady)) {
            if (old_chunk)
                ecs_unrelate(entity, RenderChunk);
            continue;
        }
        const GlobalPosition3d *position = ecs_try_get(entity, GlobalPosition3d);
        if (!position)
            continue;
        int32_t x = (int32_t)floorf(position->x / SIGPU_STATIC_CHUNK_SIZE);
        int32_t y = (int32_t)floorf(position->y / SIGPU_STATIC_CHUNK_SIZE);
        int32_t z = (int32_t)floorf(position->z / SIGPU_STATIC_CHUNK_SIZE);
        ecs_entity_t chunk = find_or_create_chunk(x, y, z);
        if (old_chunk != chunk)
            ecs_relate(entity, RenderChunk, chunk);
        mark_chunk_dirty(chunk);
    }
    cache->dirty_count = 0;
}

static void rebuild_chunk(ecs_entity_t entity, RenderChunkGpuData *gpu, RenderChunkBounds *bounds) {
    sigpu_static_chunk_t *chunk = &SIGPU_STATICRENDERCACHE->static_chunks[gpu->index];
    ecs_query_id_t sources = ecs_query({ .relations = { ecs_to(RenderChunk, entity) } });
    Uint32 source_count = ecs_query_count(sources);
    sigpu_axis_instance_t *axis[SIGPU_PRIMITIVE_COUNT];
    sigpu_rotated_instance_t *rotated[SIGPU_PRIMITIVE_COUNT];
    Uint32 axis_count[SIGPU_PRIMITIVE_COUNT] = { 0 };
    Uint32 rotated_count[SIGPU_PRIMITIVE_COUNT] = { 0 };
    for (Uint32 p = 0; p < SIGPU_PRIMITIVE_COUNT; p++) {
        axis[p] = SDL_malloc(source_count * sizeof(*axis[p]));
        rotated[p] = SDL_malloc(source_count * sizeof(*rotated[p]));
    }
    sigpu_static_chunk_t built = { 0 };
    float min_x = INFINITY, min_y = INFINITY, min_z = INFINITY;
    float max_x = -INFINITY, max_y = -INFINITY, max_z = -INFINITY;
    Uint32 total = 0;
    for (ecs_iter_t batch = ecs_query_iter(sources); ecs_iter_next(&batch);) {
        for (Uint32 i = 0; i < batch.count; i++) {
            ecs_entity_t source = batch.entities[i];
            if (!ecs_has(source, Static3dReady))
                continue;
            const GlobalPosition3d *position = ecs_try_get(source, GlobalPosition3d);
            const GlobalOrientation3d *orientation = ecs_try_get(source, GlobalOrientation3d);
            const GlobalScale3d *scale = ecs_try_get(source, GlobalScale3d);
            const Color *color = ecs_try_get(source, Color);
            if (!position || !orientation || !scale || !color)
                continue;
            const void *shapes[SIGPU_PRIMITIVE_COUNT] = {
                ecs_try_get(source, Cuboid),
                ecs_try_get(source, Cylinder),
                ecs_try_get(source, Sphere),
            };
            const Bloom *bloom_component = ecs_try_get(source, Bloom);
            float bloom = bloom_component ? fmaxf(bloom_component->intensity, 0.0f) : 0.0f;
            for (Uint32 p = 0; p < SIGPU_PRIMITIVE_COUNT; p++) {
                if (!shapes[p])
                    continue;
                primitive_size_t size =
                    scaled_size(primitive_size((sigpu_primitive_t)p, shapes[p]), *scale);
                float radius = primitive_radius((sigpu_primitive_t)p, size);
                min_x = fminf(min_x, position->x - radius);
                min_y = fminf(min_y, position->y - radius);
                min_z = fminf(min_z, position->z - radius);
                max_x = fmaxf(max_x, position->x + radius);
                max_y = fmaxf(max_y, position->y + radius);
                max_z = fmaxf(max_z, position->z + radius);
                built.primitive_radius[p] = fmaxf(built.primitive_radius[p], radius);
                built.primitive_center[p] = sigpu_vec3_add(
                    built.primitive_center[p],
                    (sigpu_vec3_t){ position->x, position->y, position->z }
                );
                if (is_rotated(*orientation))
                    rotated[p][rotated_count[p]++] =
                        make_owned_rotated(*position, *orientation, size, *color, bloom);
                else
                    axis[p][axis_count[p]++] = make_owned_axis(*position, size, *color, bloom);
                built.bloom |= bloom > 0.0f;
                total++;
            }
        }
    }
    ecs_query_fini(sources);
    if (total) {
        built.center = (sigpu_vec3_t){ (min_x + max_x) * 0.5f,
                                       (min_y + max_y) * 0.5f,
                                       (min_z + max_z) * 0.5f };
        built.radius =
            0.5f * sqrtf(
                       (max_x - min_x) * (max_x - min_x) + (max_y - min_y) * (max_y - min_y) +
                       (max_z - min_z) * (max_z - min_z)
                   );
    }
    for (Uint32 p = 0; p < SIGPU_PRIMITIVE_COUNT; p++) {
        Uint32 count = axis_count[p] + rotated_count[p];
        if (count)
            built.primitive_center[p] = sigpu_vec3_scale(built.primitive_center[p], 1.0f / count);
        gpu->axis_count[p] = axis_count[p];
        gpu->rotated_count[p] = rotated_count[p];
    }
    sigpu_static_upload_t upload = { 0 };
    for (Uint32 p = 0; p < SIGPU_PRIMITIVE_COUNT; p++) {
        upload.axis[p] = axis[p];
        upload.axis_count[p] = axis_count[p];
        upload.rotated[p] = rotated[p];
        upload.rotated_count[p] = rotated_count[p];
    }
    sigpu_static_chunk_upload(chunk, &upload);
    chunk->center = built.center;
    chunk->radius = built.radius;
    chunk->bloom = built.bloom;
    SDL_memcpy(chunk->primitive_center, built.primitive_center, sizeof(built.primitive_center));
    SDL_memcpy(chunk->primitive_radius, built.primitive_radius, sizeof(built.primitive_radius));
    bounds->center_x = built.center.x;
    bounds->center_y = built.center.y;
    bounds->center_z = built.center.z;
    bounds->radius = built.radius;
    for (Uint32 p = 0; p < SIGPU_PRIMITIVE_COUNT; p++) {
        SDL_free(axis[p]);
        SDL_free(rotated[p]);
    }
}

static void rebuild_dirty_static_chunks(ecs_iter_t *it) {
    RenderChunkBounds *bounds = ecs_field(it, 0);
    RenderChunkGpuData *gpu = ecs_field(it, 1);
    ecs_defer_begin();
    for (Uint32 i = 0; i < it->count; i++) {
        rebuild_chunk(it->entities[i], &gpu[i], &bounds[i]);
        ecs_remove(it->entities[i], RenderChunkDirty);
    }
    ecs_defer_end();
}

ecs_system_id_t sigpu_static_chunks_register(ecs_system_id_t camera_system) {
    ECS_RELATION_REGISTER(RenderChunk);
    ECS_COMPONENT_REGISTER(
        RenderChunkBounds,
        RenderChunkGpuData,
        RenderChunkVisibility,
        RenderChunkDirty
    );
    static_ready_query = ecs_query({ .components = { ecs_filter(Static3dReady) } });
    ecs_observer({ .on = EcsOnSet, .query.components = { ecs_filter(Static3dReady) },
                   .callback = sigpu_static_chunks_invalidate });
    ecs_observer({ .on = EcsOnAdd, .query.components = { ecs_filter(Static3dReady) },
                   .callback = sigpu_static_chunks_invalidate });
    ecs_observer({ .on = EcsOnRemove, .query.components = { ecs_filter(Static3dReady) },
                   .callback = sigpu_static_chunks_invalidate });
    ecs_observer({ .on = EcsOnRelationSet,
                   .query.components = { ecs_filter(Static3dReady) },
                   .callback = sigpu_static_chunks_invalidate });
    ecs_observer({ .on = EcsOnRelationRemove,
                   .query.components = { ecs_filter(Static3dReady) },
                   .callback = sigpu_static_chunks_invalidate });
    ecs_observer({ .on = EcsOnSet, .query.components = { ecs_filter(Abstract) },
                   .callback = sigpu_static_chunks_invalidate });
    ecs_observer({ .on = EcsOnAdd, .query.components = { ecs_filter(Abstract) },
                   .callback = sigpu_static_chunks_invalidate });
    ecs_observer({ .on = EcsOnRemove, .query.components = { ecs_filter(Abstract) },
                   .callback = sigpu_static_chunks_invalidate });
    ecs_system_id_t assign = ecs_system(
        {
            .name = "AssignStaticChunks",
            .phase = EcsPreRender,
            .after = { camera_system },
            .callback = assign_dirty_static_objects,
            .main_thread_only = true,
        }
    );
    return ecs_system({
        .name = "RebuildDirtyStaticChunks", .phase = EcsPreRender, .after = { assign },
        .query.components = {
            { .id = ecs_id(RenderChunkBounds), .access = EcsInOut },
            { .id = ecs_id(RenderChunkGpuData), .access = EcsInOut },
            { .id = ecs_id(RenderChunkDirty), .access = EcsFilter },
        },
        .callback = rebuild_dirty_static_chunks, .main_thread_only = true,
    });
}
