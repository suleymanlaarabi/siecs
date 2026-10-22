#include "sigpu_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Ranges empty this many frames give their storage back. */
#define GPU_RANGE_EMPTY_FRAMES 60
/* Unchanged frames and rows before a range is partitioned in cells. */
#define GPU_PARTITION_FRAMES 4
#define GPU_PARTITION_ROWS 256
#define GPU_MIN_BLOCK 16u

gpu_feature_t *gpu_feature_of(ecs_entity_t entity) {
    for (uint32_t i = 0; i < gpu_session.features.count; i++) {
        if (gpu_session.features.items[i]->entity == entity) {
            return gpu_session.features.items[i];
        }
    }
    return NULL;
}

static gpu_range_t *range_of(gpu_feature_t *feature, uint16_t table) {
    const uint32_t index = table < feature->table_slots ? feature->range_of_table[table] : 0;
    return index ? &feature->ranges.items[index - 1] : NULL;
}

/* ----------------------------------------------------------------------
 * Extraction (parallel)
 * ---------------------------------------------------------------------- */

static void atomic_min_u32(_Atomic uint32_t *value, uint32_t candidate) {
    uint32_t current = atomic_load_explicit(value, memory_order_relaxed);
    while (candidate < current &&
           !atomic_compare_exchange_weak_explicit(value, &current, candidate,
                                                  memory_order_relaxed, memory_order_relaxed)) {
    }
}

static void atomic_max_u32(_Atomic uint32_t *value, uint32_t candidate) {
    uint32_t current = atomic_load_explicit(value, memory_order_relaxed);
    while (candidate > current &&
           !atomic_compare_exchange_weak_explicit(value, &current, candidate,
                                                  memory_order_relaxed, memory_order_relaxed)) {
    }
}

static _Atomic uint32_t unresolved_rows;

/* Rows of the portion: the range they live in, or NULL when the table grew
 * after sizing (the rows are extracted next frame). */
static gpu_range_t *portion_range(ecs_iter_t *it, gpu_feature_t **feature) {
    *feature = (gpu_feature_t *)it->user_data;
    gpu_range_t *range = range_of(*feature, ecs_iter_table_id(it));
    return range && it->row_offset + it->count <= range->capacity ? range : NULL;
}

void *sigpu_instances(ecs_iter_t *it) {
    gpu_feature_t *feature;
    const gpu_range_t *range = portion_range(it, &feature);
    return range ? feature->instances + (size_t)(range->offset + it->row_offset) * feature->stride
                 : NULL;
}

GpuSphere *sigpu_bounds(ecs_iter_t *it) {
    gpu_feature_t *feature;
    const gpu_range_t *range = portion_range(it, &feature);
    return range && feature->culling ? feature->bounds + range->offset + it->row_offset : NULL;
}

uint32_t *sigpu_order_keys(ecs_iter_t *it) {
    gpu_feature_t *feature;
    const gpu_range_t *range = portion_range(it, &feature);
    return range && feature->ordering == GpuOrderLayered
               ? feature->keys + range->offset + it->row_offset
               : NULL;
}

void *sigpu_feature_data(ecs_iter_t *it) {
    return it ? ((gpu_feature_t *)it->user_data)->user_data : NULL;
}

static void extract(ecs_iter_t *it) {
    gpu_feature_t *feature;
    gpu_range_t *range = portion_range(it, &feature);
    if (!range) {
        return;
    }
    const uint32_t base = range->offset + it->row_offset;
    memcpy(feature->entities + base, it->entities, it->count * sizeof(ecs_entity_t));
    const ecs_relation_target_t *targets =
        feature->material_relation ? ecs_targets(it, DrawMaterial) : NULL;
    uint32_t unresolved = 0;
    for (uint32_t i = 0; i < it->count; i++) {
        const ecs_entity_t material =
            targets && targets[i].entity ? targets[i].entity : feature->material;
        feature->material_entities[base + i] = material;
        const uint32_t index = material ? gpu_material_index(material) : 0;
        feature->materials[base + i] = index;
        unresolved += index == GPU_NONE;
    }
    if (unresolved) {
        atomic_fetch_add_explicit(&unresolved_rows, unresolved, memory_order_relaxed);
    }
    if (feature->keys) {
        memset(feature->keys + base, 0, it->count * sizeof(uint32_t));
    }
    feature->extract(it);
    atomic_min_u32(&range->dirty_lo, it->row_offset);
    atomic_max_u32(&range->dirty_hi, it->row_offset + it->count);
}

/* ----------------------------------------------------------------------
 * Declaration
 * ---------------------------------------------------------------------- */

static bool has_relation(const ecs_query_desc_t *query, ecs_relation_id_t relation) {
    for (uint32_t i = 0; i < ECS_QUERY_RELATION_CAPACITY; i++) {
        if (query->relations[i].id == relation) {
            return true;
        }
    }
    return false;
}

static void add_relation(ecs_query_desc_t *query, ecs_relation_id_t relation) {
    for (uint32_t i = 0; i < ECS_QUERY_RELATION_CAPACITY; i++) {
        if (!query->relations[i].id) {
            query->relations[i] = (ecs_query_relation_term_t){ 0, relation, EcsRelationOptional };
            return;
        }
    }
}

ecs_entity_t sigpu_feature(const GpuFeatureDesc *desc) {
    if (!gpu_session.open || !desc || !desc->name || !desc->extract || !desc->instance) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "sigpu: a feature needs a name, an extract callback "
                                           "and an instance component");
        return 0;
    }
    const ecs_component_info_t *instance = ecs_component_info(desc->instance);
    if (!instance || !instance->size) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "sigpu: feature %s: the instance component has no data",
                     desc->name);
        return 0;
    }
    gpu_feature_t *feature = calloc(1, sizeof(gpu_feature_t));
    if (!feature || !gpu_vec_push(gpu_session.features, feature)) {
        free(feature);
        return 0;
    }
    feature->entity = ecs_new_named(0, desc->name);
    SDL_strlcpy(feature->name, desc->name, sizeof(feature->name));
    feature->instance = desc->instance;
    feature->stride = (uint32_t)instance->size;
    feature->extract = desc->extract;
    feature->user_data = desc->user_data;
    feature->user_data_dtor = desc->user_data_dtor;
    feature->culling = desc->culling;
    feature->ordering = desc->ordering;
    feature->cell_size = desc->cell_size > 0.0f ? desc->cell_size : 32.0f;
    feature->chunk_size = desc->chunk_size ? desc->chunk_size : 1024;
    feature->geometry = desc->geometry;
    feature->material = desc->material;
    feature->program = desc->program;

    ecs_query_desc_t query = desc->query;
    if (!has_relation(&query, ecs_rid(DrawGeometry))) {
        add_relation(&query, ecs_rid(DrawGeometry));
    }
    if (!has_relation(&query, ecs_rid(DrawMaterial))) {
        add_relation(&query, ecs_rid(DrawMaterial));
    }
    feature->geometry_relation = true;
    feature->material_relation = true;
    /* Sizing visits every table: the same query without change filters. */
    ecs_query_desc_t sizing = query;
    for (uint32_t i = 0; i < ECS_QUERY_TERM_CAPACITY; i++) {
        sizing.components[i].access &= ~ECS_TERM_CHANGED;
    }
    feature->sizing = ecs_query_init(&sizing);
    char name[96];
    SDL_snprintf(name, sizeof(name), "GpuExtract(%s)", desc->name);
    feature->system = ecs_system_init(&(ecs_system_desc_t){
        .name = name,
        .query = query,
        .callback = extract,
        .user_data = (uintptr_t)feature,
        .phase = GpuExtract,
        .after = { gpu_session.sizing_system },
        .parallel = true,
        .chunk_size = feature->chunk_size,
    });
    ecs_set(feature->entity, GpuFeature,
            { .instance_size = feature->stride, .culling = feature->culling,
              .ordering = feature->ordering, .geometry = feature->geometry,
              .material = feature->material, .program = feature->program,
              .cell_size = feature->cell_size });
    return feature->entity;
}

/* ----------------------------------------------------------------------
 * Storage
 * ---------------------------------------------------------------------- */

static uint32_t size_class(uint32_t rows) {
    uint32_t cls = 0;
    while ((GPU_MIN_BLOCK << cls) < rows) {
        cls++;
    }
    return cls;
}

static bool storage_grow(gpu_feature_t *feature, uint32_t needed) {
    if (needed <= feature->capacity) {
        return true;
    }
    uint32_t capacity = feature->capacity ? feature->capacity : 1024;
    while (capacity < needed) {
        capacity *= 2;
    }
#define GROW(field, size)                                                                          \
    do {                                                                                           \
        void *grown = realloc(feature->field, (size_t)capacity * (size));                          \
        if (!grown) {                                                                              \
            return false;                                                                          \
        }                                                                                          \
        feature->field = grown;                                                                    \
    } while (0)
    GROW(instances, feature->stride);
    GROW(bounds, sizeof(GpuSphere));
    GROW(entities, sizeof(ecs_entity_t));
    GROW(material_entities, sizeof(ecs_entity_t));
    GROW(materials, sizeof(uint32_t));
    if (feature->ordering == GpuOrderLayered) {
        GROW(keys, sizeof(uint32_t));
    }
#undef GROW
    const size_t old = feature->capacity;
    memset(feature->instances + old * feature->stride, 0, (capacity - old) * feature->stride);
    memset(feature->bounds + old, 0, (capacity - old) * sizeof(GpuSphere));
    feature->capacity = capacity;
    gpu_stats()->range_growths++;
    return true;
}

static bool block_alloc(gpu_feature_t *feature, uint32_t rows, uint32_t *offset,
                        uint32_t *capacity) {
    const uint32_t cls = size_class(rows);
    *capacity = GPU_MIN_BLOCK << cls;
    if (feature->free_blocks[cls].count) {
        *offset = feature->free_blocks[cls].items[--feature->free_blocks[cls].count];
        return true;
    }
    if (!storage_grow(feature, feature->used + *capacity)) {
        return false;
    }
    *offset = feature->used;
    feature->used += *capacity;
    return true;
}

static void block_free(gpu_feature_t *feature, uint32_t offset, uint32_t capacity) {
    if (capacity) {
        gpu_vec_push(feature->free_blocks[size_class(capacity)], offset);
    }
}

static void range_unpartition(gpu_range_t *range) {
    free(range->order);
    free(range->cells);
    range->order = NULL;
    range->cells = NULL;
    range->cell_count = 0;
    range->partitioned = false;
}

static void move_rows(gpu_feature_t *feature, uint32_t from, uint32_t to, uint32_t rows) {
    memmove(feature->instances + (size_t)to * feature->stride,
            feature->instances + (size_t)from * feature->stride, (size_t)rows * feature->stride);
    memmove(feature->bounds + to, feature->bounds + from, rows * sizeof(GpuSphere));
    memmove(feature->entities + to, feature->entities + from, rows * sizeof(ecs_entity_t));
    memmove(feature->material_entities + to, feature->material_entities + from,
            rows * sizeof(ecs_entity_t));
    memmove(feature->materials + to, feature->materials + from, rows * sizeof(uint32_t));
    if (feature->keys) {
        memmove(feature->keys + to, feature->keys + from, rows * sizeof(uint32_t));
    }
}

static gpu_range_t *range_add(gpu_feature_t *feature, uint16_t table) {
    if (table >= feature->table_slots) {
        uint32_t slots = feature->table_slots ? feature->table_slots : 64;
        while (slots <= table) {
            slots *= 2;
        }
        uint32_t *grown = realloc(feature->range_of_table, slots * sizeof(uint32_t));
        if (!grown) {
            return NULL;
        }
        memset(grown + feature->table_slots, 0, (slots - feature->table_slots) * sizeof(uint32_t));
        feature->range_of_table = grown;
        feature->table_slots = slots;
    }
    gpu_range_t added = { .table = table };
    atomic_init(&added.dirty_lo, UINT32_MAX);
    atomic_init(&added.dirty_hi, 0);
    if (!gpu_vec_push(feature->ranges, added)) {
        return NULL;
    }
    feature->range_of_table[table] = feature->ranges.count;
    return &feature->ranges.items[feature->ranges.count - 1];
}

/* Fits the range to count rows; moved rows keep their content. */
static bool range_fit(gpu_feature_t *feature, gpu_range_t *range, uint32_t count) {
    if (count > range->capacity) {
        uint32_t offset, capacity;
        if (!block_alloc(feature, count, &offset, &capacity)) {
            return false;
        }
        if (range->count) {
            move_rows(feature, range->offset, offset, range->count);
        }
        block_free(feature, range->offset, range->capacity);
        range->offset = offset;
        range->capacity = capacity;
        atomic_store(&range->dirty_lo, 0);
        atomic_store(&range->dirty_hi, count);
    }
    if (count != range->count) {
        range_unpartition(range);
        range->stable_frames = 0;
    }
    range->count = count;
    return true;
}

static void range_drop(gpu_feature_t *feature, uint32_t index) {
    gpu_range_t *range = &feature->ranges.items[index];
    block_free(feature, range->offset, range->capacity);
    range_unpartition(range);
    feature->range_of_table[range->table] = 0;
    const uint32_t last = feature->ranges.count - 1;
    if (index != last) {
        feature->ranges.items[index] = feature->ranges.items[last];
        feature->range_of_table[feature->ranges.items[index].table] = index + 1;
    }
    feature->ranges.count--;
}

/* GpuExtract, before the extraction systems: every range fits its table.
 * The query lists the non-empty tables with their counts (RFC 6.2). */
void gpu_features_size(ecs_iter_t *it) {
    (void)it;
    const uint64_t start = SDL_GetTicksNS();
    for (uint32_t f = 0; f < gpu_session.features.count; f++) {
        gpu_feature_t *feature = gpu_session.features.items[f];
        for (uint32_t r = 0; r < feature->ranges.count; r++) {
            feature->ranges.items[r].live = false;
        }
        for (ecs_iter_t rows = ecs_query_iter(feature->sizing); ecs_iter_next(&rows);) {
            const uint16_t table = ecs_iter_table_id(&rows);
            gpu_range_t *range = range_of(feature, table);
            if (!range) {
                range = range_add(feature, table);
                if (!range) {
                    continue;
                }
            }
            const uint32_t count = rows.row_offset + rows.count;
            if (!range->live || count > range->count) {
                range->live = true;
                const ecs_entity_t geometry = ecs_target_shared(&rows, DrawGeometry);
                range->geometry = geometry ? geometry : feature->geometry;
                if (!range_fit(feature, range, count)) {
                    SDL_LogError(SDL_LOG_CATEGORY_GPU, "sigpu: feature %s: out of memory",
                                 feature->name);
                }
            }
        }
        for (uint32_t r = 0; r < feature->ranges.count;) {
            gpu_range_t *range = &feature->ranges.items[r];
            if (range->live) {
                range->empty_frames = 0;
                r++;
                continue;
            }
            if (range->count) {
                range->count = 0;
                range_unpartition(range);
                feature->revision++;
            }
            if (++range->empty_frames >= GPU_RANGE_EMPTY_FRAMES) {
                range_drop(feature, r);
            } else {
                r++;
            }
        }
    }
    gpu_stats()->extract_ns += SDL_GetTicksNS() - start;
}

/* ----------------------------------------------------------------------
 * Preparation: uploads, material fix-ups, partitions
 * ---------------------------------------------------------------------- */

static uint32_t spread(uint32_t value) {
    value &= 0x3ff;
    value = (value | (value << 16)) & 0x030000ff;
    value = (value | (value << 8)) & 0x0300f00f;
    value = (value | (value << 4)) & 0x030c30c3;
    value = (value | (value << 2)) & 0x09249249;
    return value;
}

static const uint32_t *sort_codes;

static int code_compare(const void *a, const void *b) {
    const uint32_t left = sort_codes[*(const uint32_t *)a];
    const uint32_t right = sort_codes[*(const uint32_t *)b];
    if (left != right) {
        return left < right ? -1 : 1;
    }
    return *(const uint32_t *)a < *(const uint32_t *)b ? -1 : 1;
}

/* Rows of a stable range sorted by the Morton code of their cell; each run
 * of one code is a cell with its bounding sphere (RFC section 6.4). */
static void partition(gpu_feature_t *feature, gpu_range_t *range) {
    const GpuSphere *bounds = feature->bounds + range->offset;
    uint32_t *codes = malloc(range->count * sizeof(uint32_t));
    range->order = malloc(range->count * sizeof(uint32_t));
    if (!codes || !range->order) {
        free(codes);
        range_unpartition(range);
        return;
    }
    float min[3] = { INFINITY, INFINITY, INFINITY };
    for (uint32_t i = 0; i < range->count; i++) {
        min[0] = fminf(min[0], bounds[i].x);
        min[1] = fminf(min[1], bounds[i].y);
        min[2] = fminf(min[2], bounds[i].z);
    }
    const float cell = feature->cell_size;
    for (uint32_t i = 0; i < range->count; i++) {
        const uint32_t x = (uint32_t)fminf((bounds[i].x - min[0]) / cell, 1023.0f);
        const uint32_t y = (uint32_t)fminf((bounds[i].y - min[1]) / cell, 1023.0f);
        const uint32_t z = (uint32_t)fminf((bounds[i].z - min[2]) / cell, 1023.0f);
        codes[i] = spread(x) | spread(y) << 1 | spread(z) << 2;
        range->order[i] = i;
    }
    sort_codes = codes;
    qsort(range->order, range->count, sizeof(uint32_t), code_compare);
    uint32_t cells = 0;
    for (uint32_t i = 0; i < range->count; i++) {
        cells += i == 0 || codes[range->order[i]] != codes[range->order[i - 1]];
    }
    range->cells = malloc(cells * sizeof(gpu_cell_t));
    if (!range->cells) {
        free(codes);
        range_unpartition(range);
        return;
    }
    range->cell_count = 0;
    for (uint32_t i = 0; i < range->count;) {
        uint32_t end = i + 1;
        while (end < range->count && codes[range->order[end]] == codes[range->order[i]]) {
            end++;
        }
        float lo[3] = { INFINITY, INFINITY, INFINITY }, hi[3] = { -INFINITY, -INFINITY, -INFINITY };
        for (uint32_t j = i; j < end; j++) {
            const GpuSphere *s = &bounds[range->order[j]];
            const float c[3] = { s->x, s->y, s->z };
            for (int k = 0; k < 3; k++) {
                lo[k] = fminf(lo[k], c[k] - s->radius);
                hi[k] = fmaxf(hi[k], c[k] + s->radius);
            }
        }
        gpu_cell_t *out = &range->cells[range->cell_count++];
        out->center[0] = (lo[0] + hi[0]) * 0.5f;
        out->center[1] = (lo[1] + hi[1]) * 0.5f;
        out->center[2] = (lo[2] + hi[2]) * 0.5f;
        out->radius = 0.0f;
        for (uint32_t j = i; j < end; j++) {
            const GpuSphere *s = &bounds[range->order[j]];
            const float dx = s->x - out->center[0], dy = s->y - out->center[1],
                        dz = s->z - out->center[2];
            out->radius = fmaxf(out->radius, sqrtf(dx * dx + dy * dy + dz * dz) + s->radius);
        }
        out->first = i;
        out->count = end - i;
        i = end;
    }
    free(codes);
    range->partitioned = true;
    gpu_stats()->partitions_built++;
}

static void fix_rows(gpu_feature_t *feature, bool retargeted_only, ecs_entity_t entity) {
    for (uint32_t r = 0; r < feature->ranges.count; r++) {
        const gpu_range_t *range = &feature->ranges.items[r];
        for (uint32_t i = range->offset; i < range->offset + range->count; i++) {
            if (retargeted_only) {
                if (feature->entities[i] != entity) {
                    continue;
                }
                const ecs_entity_t target = ecs_is_alive(entity) ? ecs_target(entity, DrawMaterial)
                                                                 : 0;
                feature->material_entities[i] = target ? target : feature->material;
                feature->revision++;
            } else if (feature->materials[i] != GPU_NONE) {
                continue;
            }
            const ecs_entity_t material = feature->material_entities[i];
            feature->materials[i] = material ? gpu_material_index(material) : 0;
            feature->unresolved += feature->materials[i] == GPU_NONE;
        }
    }
}

static bool feature_buffer(gpu_feature_t *feature) {
    if (feature->buffer && feature->buffer_capacity >= feature->capacity) {
        return true;
    }
    if (feature->buffer) {
        gpu_uploads_drop_raw(feature->buffer);
        gpu_release_buffer(feature->buffer);
        gpu_stats()->memory_reserved -= (uint64_t)feature->buffer_capacity * feature->stride;
    }
    feature->buffer = NULL;
    feature->buffer_capacity = 0;
    if (!feature->capacity) {
        return false;
    }
    feature->buffer = gpu_create_buffer_flags(feature->capacity * feature->stride,
                                              SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);
    if (!feature->buffer) {
        return false;
    }
    feature->buffer_capacity = feature->capacity;
    gpu_stats()->memory_reserved += (uint64_t)feature->capacity * feature->stride;
    feature->upload_all = true;
    return true;
}

static void upload_rows(gpu_feature_t *feature, uint32_t first, uint32_t count) {
    const uint32_t bytes = count * feature->stride;
    gpu_upload_raw(feature->buffer, first * feature->stride,
                   feature->instances + (size_t)first * feature->stride, bytes);
    GpuStats *stats = gpu_stats();
    stats->instance_uploads++;
    stats->instance_upload_bytes += bytes;
}

void gpu_features_prepare(void) {
    GpuStats *stats = gpu_stats();
    const bool unresolved = atomic_exchange(&unresolved_rows, 0) != 0;
    for (uint32_t f = 0; f < gpu_session.features.count; f++) {
        gpu_feature_t *feature = gpu_session.features.items[f];
        if (unresolved || feature->unresolved) {
            feature->unresolved = 0;
            fix_rows(feature, false, 0);
        }
        for (uint32_t i = 0; i < gpu_session.retargeted.count; i++) {
            fix_rows(feature, true, gpu_session.retargeted.items[i]);
        }
        const bool buffer = feature_buffer(feature);
        for (uint32_t r = 0; r < feature->ranges.count; r++) {
            gpu_range_t *range = &feature->ranges.items[r];
            const uint32_t lo = atomic_exchange(&range->dirty_lo, UINT32_MAX);
            const uint32_t hi = atomic_exchange(&range->dirty_hi, 0);
            if (hi > lo) {
                stats->tables_extracted++;
                feature->revision++;
                range->stable_frames = 0;
                range_unpartition(range);
                if (buffer && !feature->upload_all) {
                    const uint32_t end = hi < range->count ? hi : range->count;
                    if (end > lo) {
                        upload_rows(feature, range->offset + lo, end - lo);
                    }
                }
                continue;
            }
            if (range->count) {
                stats->tables_skipped++;
            }
            if (range->stable_frames < UINT32_MAX) {
                range->stable_frames++;
            }
            if (feature->culling && !range->partitioned &&
                range->stable_frames >= GPU_PARTITION_FRAMES &&
                range->count >= GPU_PARTITION_ROWS) {
                partition(feature, range);
            }
        }
        if (buffer && feature->upload_all && feature->used) {
            upload_rows(feature, 0, feature->used);
            feature->upload_all = false;
        }
        stats->memory_used += (uint64_t)feature->used * feature->stride;
    }
    gpu_session.retargeted.count = 0;
}

void gpu_features_release(void) {
    for (uint32_t f = 0; f < gpu_session.features.count; f++) {
        gpu_feature_t *feature = gpu_session.features.items[f];
        if (feature->buffer) {
            gpu_uploads_drop_raw(feature->buffer);
            gpu_release_buffer(feature->buffer);
            gpu_stats()->memory_reserved -= (uint64_t)feature->buffer_capacity * feature->stride;
        }
        feature->buffer = NULL;
        feature->buffer_capacity = 0;
        feature->upload_all = true;
    }
}

void gpu_features_fini(void) {
    for (uint32_t f = 0; f < gpu_session.features.count; f++) {
        gpu_feature_t *feature = gpu_session.features.items[f];
        for (uint32_t r = 0; r < feature->ranges.count; r++) {
            range_unpartition(&feature->ranges.items[r]);
        }
        for (uint32_t c = 0; c < 32; c++) {
            gpu_vec_free(feature->free_blocks[c]);
        }
        gpu_vec_free(feature->ranges);
        free(feature->instances);
        free(feature->bounds);
        free(feature->entities);
        free(feature->material_entities);
        free(feature->materials);
        free(feature->keys);
        free(feature->range_of_table);
        if (feature->user_data_dtor) {
            feature->user_data_dtor(feature->user_data);
        }
        free(feature);
    }
    gpu_vec_free(gpu_session.features);
    gpu_vec_free(gpu_session.retargeted);
}

/* ----------------------------------------------------------------------
 * Picking (RFC section 12.4)
 * ---------------------------------------------------------------------- */

static bool ray_hits(const float origin[3], const float direction[3], const float center[3],
                     float radius) {
    const float o[3] = { origin[0] - center[0], origin[1] - center[1], origin[2] - center[2] };
    const float along = -(o[0] * direction[0] + o[1] * direction[1] + o[2] * direction[2]);
    const float closest = o[0] * o[0] + o[1] * o[1] + o[2] * o[2] - along * along;
    return closest <= radius * radius && along + sqrtf(fmaxf(radius * radius - closest, 0.0f)) >= 0;
}

typedef struct {
    ecs_entity_t entity;
    float distance;
    bool hit;
} pick_best_t;

static void pick_row(gpu_feature_t *feature, uint32_t row, const float origin[3],
                     const float direction[3], gpu_pick_fn exact, void *user_data,
                     pick_best_t *best) {
    const GpuSphere *s = &feature->bounds[row];
    const float center[3] = { s->x, s->y, s->z };
    if (!ray_hits(origin, direction, center, s->radius)) {
        return;
    }
    const ecs_entity_t entity = feature->entities[row];
    if (!ecs_is_alive(entity)) {
        return;
    }
    float distance = 0.0f;
    if (exact && !exact(entity, &distance, user_data)) {
        return;
    }
    if (!exact) {
        distance = (s->x - origin[0]) * direction[0] + (s->y - origin[1]) * direction[1] +
                   (s->z - origin[2]) * direction[2];
    }
    if (!best->hit || distance < best->distance ||
        (distance == best->distance && entity < best->entity)) {
        *best = (pick_best_t){ entity, distance, true };
    }
}

ecs_entity_t sigpu_feature_raycast(ecs_entity_t feature_entity, const float origin[3],
                                   const float direction[3], gpu_pick_fn exact, void *user_data,
                                   float *distance) {
    gpu_feature_t *feature = gpu_feature_of(feature_entity);
    pick_best_t best = { 0 };
    if (!feature || !feature->culling || !origin || !direction) {
        return 0;
    }
    for (uint32_t r = 0; r < feature->ranges.count; r++) {
        const gpu_range_t *range = &feature->ranges.items[r];
        if (range->partitioned) {
            for (uint32_t c = 0; c < range->cell_count; c++) {
                const gpu_cell_t *cell = &range->cells[c];
                if (!ray_hits(origin, direction, cell->center, cell->radius)) {
                    continue;
                }
                for (uint32_t i = cell->first; i < cell->first + cell->count; i++) {
                    pick_row(feature, range->offset + range->order[i], origin, direction, exact,
                             user_data, &best);
                }
            }
            continue;
        }
        for (uint32_t i = 0; i < range->count; i++) {
            pick_row(feature, range->offset + i, origin, direction, exact, user_data, &best);
        }
    }
    if (distance && best.hit) {
        *distance = best.distance;
    }
    return best.entity;
}
