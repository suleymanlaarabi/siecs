#include "sigpu_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
 * Culling, batching and drawing of the features in geometry passes (RFC
 * sections 6.4 to 6.6). Every geometry pass of a valid graph gets, in
 * GpuPrepare, a list of batches; each batch is one instanced draw over a
 * run of the frame's visible list: (instance index, material slot) pairs
 * read by the vertex shader as per-instance attributes "visible" and
 * "material".
 */

typedef struct {
    uint32_t instance;
    uint32_t material;
} gpu_visible_t;

typedef struct {
    gpu_feature_t *feature;
    const gpu_record_t *program;
    uint32_t stage;
    ecs_entity_t geometry;
    uint32_t lod;
    uint32_t texset;
    SDL_GPUGraphicsPipeline *pipeline;
    gpu_params_t *params;
    uint32_t first;
    uint32_t count;
} gpu_batch_t;

/* A visible row before grouping. */
typedef struct {
    uint64_t sort;
    uint32_t batch;
    gpu_visible_t visible;
} gpu_item_t;

static GPU_VEC(gpu_batch_t) batches;
static GPU_VEC(gpu_visible_t) visible;
static GPU_VEC(gpu_visible_t) uploaded;
static GPU_VEC(gpu_item_t) items;
static GPU_VEC(gpu_batch_t) groups;
static GPU_VEC(uint32_t) counts;
static SDL_GPUBuffer *visible_buffer;
static uint32_t visible_capacity;

/* ----------------------------------------------------------------------
 * Views
 * ---------------------------------------------------------------------- */

typedef struct {
    float planes[6][4];
    const GpuView *desc;
} gpu_view_t;

static void view_planes(gpu_view_t *view) {
    const float *m = view->desc->view_projection;
    /* Rows of the column-major matrix; depth 0..1 (near: row 2 alone). */
    const float rows[4][4] = {
        { m[0], m[4], m[8], m[12] },
        { m[1], m[5], m[9], m[13] },
        { m[2], m[6], m[10], m[14] },
        { m[3], m[7], m[11], m[15] },
    };
    for (int k = 0; k < 4; k++) {
        view->planes[0][k] = rows[3][k] + rows[0][k];
        view->planes[1][k] = rows[3][k] - rows[0][k];
        view->planes[2][k] = rows[3][k] + rows[1][k];
        view->planes[3][k] = rows[3][k] - rows[1][k];
        view->planes[4][k] = rows[2][k];
        view->planes[5][k] = rows[3][k] - rows[2][k];
    }
    for (int p = 0; p < 6; p++) {
        float *plane = view->planes[p];
        const float length = sqrtf(plane[0] * plane[0] + plane[1] * plane[1] + plane[2] * plane[2]);
        if (length > 0.0f) {
            for (int k = 0; k < 4; k++) {
                plane[k] /= length;
            }
        }
    }
}

static bool sphere_visible(const gpu_view_t *view, float x, float y, float z, float radius) {
    for (int p = 0; p < 6; p++) {
        const float *plane = view->planes[p];
        if (plane[0] * x + plane[1] * y + plane[2] * z + plane[3] < -radius) {
            return false;
        }
    }
    return true;
}

static float view_depth(const gpu_view_t *view, const GpuSphere *s) {
    const GpuView *desc = view->desc;
    return (s->x - desc->position[0]) * desc->forward[0] +
           (s->y - desc->position[1]) * desc->forward[1] +
           (s->z - desc->position[2]) * desc->forward[2];
}

static uint32_t choose_lod(const gpu_view_t *view, const GpuGeometry *geometry, const GpuSphere *s) {
    if (!view || geometry->lod_count < 2) {
        return 0;
    }
    float pixels = s->radius * view->desc->lod_scale;
    if (!view->desc->orthographic) {
        pixels /= fmaxf(view_depth(view, s), 0.001f);
    }
    for (uint32_t i = 0; i + 1 < geometry->lod_count; i++) {
        if (geometry->lods[i].max_pixels <= 0.0f || pixels < geometry->lods[i].max_pixels) {
            return i;
        }
    }
    return geometry->lod_count - 1u;
}

/* ----------------------------------------------------------------------
 * Batching
 * ---------------------------------------------------------------------- */

static GpuVertexLayout feature_layout(const GpuGeometry *geometry) {
    GpuVertexLayout layout = { .strides = { geometry->stride, sizeof(gpu_visible_t) },
                               .instanced = { false, true } };
    for (uint32_t i = 0; i < geometry->attribute_count && i < GPU_GEOMETRY_ATTRIBUTES; i++) {
        layout.attributes[layout.attribute_count++] = geometry->attributes[i];
    }
    layout.attributes[layout.attribute_count++] =
        (GpuVertexAttribute){ .name = "visible", .format = GpuVertexUint, .buffer = 1, .offset = 0 };
    layout.attributes[layout.attribute_count++] =
        (GpuVertexAttribute){ .name = "material", .format = GpuVertexUint, .buffer = 1,
                              .offset = 4 };
    return layout;
}

static bool same_group(const gpu_batch_t *a, const gpu_batch_t *b) {
    return a->feature == b->feature && a->program == b->program && a->stage == b->stage &&
           a->geometry == b->geometry && a->lod == b->lod && a->texset == b->texset;
}

/* Group id of a row's draw state within the pass, created on first use. */
static uint32_t group_of(const gpu_batch_t *key) {
    for (uint32_t i = groups.count; i-- > 0;) {
        if (same_group(&groups.items[i], key)) {
            return i;
        }
    }
    gpu_vec_push(groups, *key);
    return groups.count - 1;
}

static void add_row(const gpu_view_t *view, ecs_entity_t phase, gpu_feature_t *feature,
                    const gpu_range_t *range, const GpuGeometry *geometry, uint32_t row) {
    const uint32_t index = range->offset + row;
    const uint32_t material_index = feature->materials[index];
    const gpu_material_t *material = gpu_material_at(material_index);
    ecs_entity_t program_entity = feature->program;
    uint32_t slot = 0, texset = 0;
    gpu_params_t *params = NULL;
    if (material_index == GPU_NONE || (material_index && (!material || !material->ready))) {
        gpu_stats()->batches_skipped++;
        gpu_stats()->skipped_material++;
        return;
    }
    if (material_index) {
        program_entity = material->program;
        slot = material->slot;
        texset = material->texset;
        params = gpu_params_for_program(program_entity);
    }
    const gpu_record_t *program = gpu_resource_record(program_entity, GPU_RESOURCE_PROGRAM);
    const uint32_t stage = program ? gpu_program_stage(program, phase) : GPU_NONE;
    if (stage == GPU_NONE) {
        return;
    }
    const GpuSphere *bounds = &feature->bounds[index];
    const gpu_batch_t key = {
        .feature = feature,
        .program = program,
        .stage = stage,
        .geometry = range->geometry,
        .lod = choose_lod(view, geometry, bounds),
        .texset = texset,
        .params = params,
    };
    gpu_item_t item = { .batch = group_of(&key), .visible = { index, slot } };
    if (feature->ordering == GpuOrderSorted && view) {
        /* Back to front: farthest first. */
        const float depth = view_depth(view, bounds);
        uint32_t bits;
        memcpy(&bits, &depth, sizeof(bits));
        bits = bits & 0x80000000u ? ~bits : bits | 0x80000000u;
        item.sort = (uint64_t)~bits << 32 | row;
    } else if (feature->ordering == GpuOrderLayered) {
        item.sort = (uint64_t)feature->keys[index] << 32 | (uint32_t)items.count;
    }
    gpu_vec_push(items, item);
    gpu_stats()->visible++;
}

static void cull_feature(const gpu_view_t *view, ecs_entity_t phase, gpu_feature_t *feature) {
    GpuStats *stats = gpu_stats();
    for (uint32_t r = 0; r < feature->ranges.count; r++) {
        const gpu_range_t *range = &feature->ranges.items[r];
        if (!range->count) {
            continue;
        }
        const ecs_entity_t geometry_entity = range->geometry;
        const GpuGeometry *geometry = geometry_entity && ecs_is_alive(geometry_entity)
                                          ? ecs_try_get(geometry_entity, GpuGeometry)
                                          : NULL;
        if (!geometry || !gpu_resource_record(geometry_entity, GPU_RESOURCE_GEOMETRY)) {
            stats->batches_skipped++;
            stats->skipped_geometry++;
            continue;
        }
        stats->candidates += range->count;
        const bool cull = view && feature->culling;
        if (cull && range->partitioned) {
            for (uint32_t c = 0; c < range->cell_count; c++) {
                const gpu_cell_t *cell = &range->cells[c];
                stats->cells_tested++;
                if (!sphere_visible(view, cell->center[0], cell->center[1], cell->center[2],
                                    cell->radius)) {
                    continue;
                }
                for (uint32_t i = cell->first; i < cell->first + cell->count; i++) {
                    add_row(view, phase, feature, range, geometry, range->order[i]);
                }
            }
            continue;
        }
        for (uint32_t row = 0; row < range->count; row++) {
            const GpuSphere *s = &feature->bounds[range->offset + row];
            if (cull && !sphere_visible(view, s->x, s->y, s->z, s->radius)) {
                continue;
            }
            add_row(view, phase, feature, range, geometry, row);
        }
    }
}

static int item_compare(const void *a, const void *b) {
    const gpu_item_t *left = a, *right = b;
    return left->sort < right->sort ? -1 : left->sort > right->sort;
}

/* The instance component must have the layout of the shader's instance
 * buffer (RFC section 10.3): the storage is uploaded as it is. */
static bool instance_layout_matches(const gpu_batch_t *batch) {
    const gpu_pack_t *pack = gpu_program_pack(batch->program);
    const gpu_program_stage_t *stage = &batch->program->as.program.stages[batch->stage];
    const uint32_t entries[2] = { stage->vertex_entry, stage->fragment_entry };
    for (uint32_t e = 0; e < 2; e++) {
        const gpu_pack_entry_t *entry = gpu_pack_entry_at(pack, entries[e]);
        for (uint32_t r = 0; entry && r < entry->resource_count; r++) {
            const gpu_pack_resource_t *resource = &pack->resources[entry->first_resource + r];
            if (resource->scope == GpuPackScopeInstance && resource->kind == GpuPackStorageBuffer &&
                resource->stride != batch->feature->stride) {
                static uint32_t logged;
                if (logged++ < 8) {
                    SDL_LogError(SDL_LOG_CATEGORY_GPU,
                                 "sigpu: feature %s: instance size %u, shader '%s' stride %u",
                                 batch->feature->name, batch->feature->stride, resource->name,
                                 resource->stride);
                }
                return false;
            }
        }
    }
    return true;
}

static void emit(const gpu_batch_t *group, const gpu_targets_t *targets) {
    gpu_batch_t batch = *group;
    if (!instance_layout_matches(&batch)) {
        gpu_stats()->batches_skipped++;
        gpu_stats()->skipped_layout++;
        return;
    }
    const GpuGeometry *geometry = ecs_get(batch.geometry, GpuGeometry);
    const GpuVertexLayout layout = feature_layout(geometry);
    batch.pipeline = gpu_pipeline_get(batch.program, batch.stage, targets, &layout, true);
    if (!batch.pipeline) {
        gpu_stats()->batches_skipped++;
        gpu_stats()->skipped_pipeline++;
        return;
    }
    gpu_vec_push(batches, batch);
    gpu_stats()->batches++;
}

/* Batched features: rows of one group are contiguous, groups in creation
 * order (counting sort). */
static void emit_batched(uint32_t first_item, const gpu_targets_t *targets) {
    const uint32_t item_count = items.count - first_item;
    gpu_vec_grow(counts, groups.count + 1);
    memset(counts.items, 0, (groups.count + 1) * sizeof(uint32_t));
    for (uint32_t i = first_item; i < items.count; i++) {
        counts.items[items.items[i].batch + 1]++;
    }
    for (uint32_t g = 0; g < groups.count; g++) {
        counts.items[g + 1] += counts.items[g];
    }
    const uint32_t base = visible.count;
    gpu_vec_grow(visible, base + item_count);
    visible.count = base + item_count;
    for (uint32_t g = 0; g < groups.count; g++) {
        groups.items[g].first = base + counts.items[g];
        groups.items[g].count = 0;
    }
    for (uint32_t i = first_item; i < items.count; i++) {
        gpu_batch_t *group = &groups.items[items.items[i].batch];
        visible.items[group->first + group->count++] = items.items[i].visible;
    }
    for (uint32_t g = 0; g < groups.count; g++) {
        if (groups.items[g].count) {
            emit(&groups.items[g], targets);
        }
    }
}

/* Sorted and layered: in key order; adjacent rows of one group merge. */
static void emit_ordered(uint32_t first_item, const gpu_targets_t *targets) {
    if (items.count <= first_item) {
        return;
    }
    qsort(items.items + first_item, items.count - first_item, sizeof(gpu_item_t), item_compare);
    gpu_batch_t current = { 0 };
    bool open = false;
    for (uint32_t i = first_item; i < items.count; i++) {
        const gpu_batch_t *group = &groups.items[items.items[i].batch];
        if (open && same_group(&current, group)) {
            current.count++;
        } else {
            if (open) {
                emit(&current, targets);
            }
            current = *group;
            current.first = visible.count;
            current.count = 1;
            open = true;
        }
        gpu_vec_push(visible, items.items[i].visible);
    }
    if (open) {
        emit(&current, targets);
    }
}

bool gpu_pass_targets(const gpu_plan_pass_t *pass, gpu_targets_t *targets) {
    *targets = (gpu_targets_t){ .depth = SDL_GPU_TEXTUREFORMAT_INVALID, .samples = 1 };
    for (uint32_t i = 0; i < pass->io.count; i++) {
        const GpuAccess *access = &pass->io.items[i];
        if (access->usage != GpuUseColor && access->usage != GpuUseDepth) {
            continue;
        }
        SDL_GPUTextureFormat format;
        if (gpu_resource_kind(access->resource) == GPU_RESOURCE_SURFACE) {
            const gpu_record_t *surface =
                gpu_resource_record(access->resource, GPU_RESOURCE_SURFACE);
            if (!surface) {
                return false;
            }
            format = surface->as.surface.format;
        } else {
            gpu_texture_info_t info;
            if (!gpu_resource_texture(access->resource, &info)) {
                return false;
            }
            format = gpu_sdl_format(info.format);
            targets->samples = info.samples ? info.samples : 1;
        }
        if (access->usage == GpuUseDepth) {
            targets->depth = format;
        } else if (targets->color_count < GPU_MAX_COLOR_TARGETS) {
            targets->colors[targets->color_count++] = format;
        }
    }
    return true;
}

static uint64_t mix(uint64_t hash, uint64_t value) {
    return hash ^ (value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2));
}

/* What a cached view's attachments depend on: the view, the content of
 * every feature, the materials and the attachments themselves. */
static uint64_t cache_signature(const gpu_plan_pass_t *pass, const GpuView *view) {
    uint64_t hash = 0;
    const uint32_t *words = (const uint32_t *)view->view_projection;
    for (uint32_t i = 0; i < 16; i++) {
        hash = mix(hash, words[i]);
    }
    for (uint32_t f = 0; f < gpu_session.features.count; f++) {
        hash = mix(hash, gpu_session.features.items[f]->revision);
    }
    hash = mix(hash, gpu_stats()->materials_prepared);
    for (uint32_t i = 0; i < pass->io.count; i++) {
        const GpuResourceState *state = gpu_state(pass->io.items[i].resource);
        hash = mix(hash, state ? state->allocation : 0);
    }
    return hash;
}

static void prepare_pass(gpu_plan_pass_t *pass) {
    pass->first_batch = batches.count;
    pass->batch_count = 0;
    pass->skip = false;
    gpu_targets_t targets;
    if (!pass->desc.phase || pass->desc.kind != GpuPassRender || !gpu_pass_targets(pass, &targets)) {
        return;
    }
    gpu_view_t view_storage;
    const gpu_view_t *view = NULL;
    const GpuView *desc = pass->desc.view && ecs_is_alive(pass->desc.view)
                              ? ecs_try_get(pass->desc.view, GpuView)
                              : NULL;
    if (desc) {
        view_storage.desc = desc;
        view_planes(&view_storage);
        view = &view_storage;
        if (desc->cache) {
            const uint64_t signature = cache_signature(pass, desc);
            if (pass->cache_valid && pass->cache_signature == signature) {
                pass->skip = true;
                return;
            }
            pass->cache_signature = signature;
            pass->cache_valid = true;
        }
    }
    const GpuStats before = *gpu_stats();
    /* Batched features, then sorted ones, then every layered feature
     * merged in key order (RFC section 6.5). */
    for (uint8_t ordering = GpuOrderBatched; ordering <= GpuOrderLayered; ordering++) {
        const uint32_t first_item = items.count;
        for (uint32_t f = 0; f < gpu_session.features.count; f++) {
            gpu_feature_t *feature = gpu_session.features.items[f];
            if (feature->ordering != ordering) {
                continue;
            }
            groups.count = 0;
            items.count = first_item;
            if (ordering == GpuOrderLayered) {
                continue;
            }
            cull_feature(view, pass->desc.phase, feature);
            if (ordering == GpuOrderBatched) {
                emit_batched(first_item, &targets);
            } else {
                emit_ordered(first_item, &targets);
            }
            items.count = first_item;
        }
        if (ordering == GpuOrderLayered) {
            groups.count = 0;
            for (uint32_t f = 0; f < gpu_session.features.count; f++) {
                if (gpu_session.features.items[f]->ordering == GpuOrderLayered) {
                    cull_feature(view, pass->desc.phase, gpu_session.features.items[f]);
                }
            }
            emit_ordered(first_item, &targets);
            items.count = first_item;
        }
    }
    pass->batch_count = batches.count - pass->first_batch;
    if (desc) {
        const GpuStats *after = gpu_stats();
        const GpuViewStats view_stats = {
            .candidates = after->candidates - before.candidates,
            .visible = after->visible - before.visible,
            .cells_tested = after->cells_tested - before.cells_tested,
            .batches = pass->batch_count,
        };
        const GpuViewStats *current = ecs_try_get(pass->desc.view, GpuViewStats);
        if (!current || memcmp(current, &view_stats, sizeof(view_stats)) != 0) {
            ecs_set_cid(pass->desc.view, ecs_id(GpuViewStats), &view_stats);
        }
    }
}

static bool visible_upload(void) {
    const uint32_t bytes = visible.count * sizeof(gpu_visible_t);
    if (!bytes) {
        uploaded.count = 0;
        return true;
    }
    if (visible_capacity < visible.count) {
        if (visible_buffer) {
            gpu_uploads_drop_raw(visible_buffer);
            gpu_release_buffer(visible_buffer);
            gpu_stats()->memory_reserved -= visible_capacity * sizeof(gpu_visible_t);
        }
        uint32_t capacity = visible_capacity ? visible_capacity : 4096;
        while (capacity < visible.count) {
            capacity *= 2;
        }
        visible_buffer = gpu_create_buffer_flags(capacity * sizeof(gpu_visible_t),
                                                 SDL_GPU_BUFFERUSAGE_VERTEX);
        visible_capacity = visible_buffer ? capacity : 0;
        gpu_stats()->memory_reserved += visible_capacity * sizeof(gpu_visible_t);
        uploaded.count = 0;
        if (!visible_buffer) {
            return false;
        }
    }
    /* A still scene and camera upload nothing (RFC section 1.3). */
    if (uploaded.count == visible.count &&
        memcmp(uploaded.items, visible.items, bytes) == 0) {
        return true;
    }
    if (!gpu_vec_grow(uploaded, visible.count)) {
        return false;
    }
    memcpy(uploaded.items, visible.items, bytes);
    uploaded.count = visible.count;
    return gpu_upload_raw(visible_buffer, 0, uploaded.items, bytes);
}

void gpu_batches_prepare_all(void) {
    batches.count = 0;
    visible.count = 0;
    for (uint32_t g = 0; g < gpu_session.graphs.count; g++) {
        gpu_graph_t *graph = &gpu_session.graphs.items[g];
        for (uint32_t p = 0; graph->valid && p < graph->plan.count; p++) {
            prepare_pass(&graph->plan.items[p]);
        }
    }
    if (!visible_upload()) {
        for (uint32_t g = 0; g < gpu_session.graphs.count; g++) {
            gpu_graph_t *graph = &gpu_session.graphs.items[g];
            for (uint32_t p = 0; p < graph->plan.count; p++) {
                graph->plan.items[p].batch_count = 0;
            }
        }
    }
}

/* ----------------------------------------------------------------------
 * Recording
 * ---------------------------------------------------------------------- */

void gpu_draw_batches(gpu_pass_ctx_t *ctx, gpu_plan_pass_t *pass) {
    for (uint32_t b = pass->first_batch; b < pass->first_batch + pass->batch_count; b++) {
        const gpu_batch_t *batch = &batches.items[b];
        const GpuGeometry *geometry = ecs_try_get(batch->geometry, GpuGeometry);
        SDL_GPUBuffer *vertices = geometry ? gpu_resource_buffer(geometry->vertices) : NULL;
        SDL_GPUBuffer *indices =
            geometry && geometry->index_size ? gpu_resource_buffer(geometry->indices) : NULL;
        if (!vertices || (geometry->index_size && !indices) ||
            !gpu_ctx_pipeline(ctx, batch->program, batch->stage, batch->pipeline)) {
            gpu_stats()->batches_skipped++;
            continue;
        }
        gpu_binding_t extra[2 + 8];
        uint32_t extra_count = 0;
        extra[extra_count++] = (gpu_binding_t){ .scope = GpuPackScopeInstance,
                                                .buffer = batch->feature->buffer };
        if (batch->params && batch->params->buffer) {
            extra[extra_count++] = (gpu_binding_t){ .scope = GpuPackScopeMaterial,
                                                    .buffer = batch->params->buffer };
        }
        const gpu_texset_t *set = gpu_texset(batch->texset);
        for (uint32_t t = 0; set && t < set->count; t++) {
            gpu_binding_t binding = { .name = set->bindings[t].name };
            binding.texture = set->bindings[t].texture
                                  ? gpu_resource_texture(set->bindings[t].texture, NULL)
                                  : gpu_default_texture();
            const gpu_record_t *sampler =
                set->bindings[t].sampler
                    ? gpu_resource_record(set->bindings[t].sampler, GPU_RESOURCE_SAMPLER)
                    : NULL;
            binding.sampler = sampler ? sampler->as.sampler : gpu_default_sampler();
            extra[extra_count++] = binding;
        }
        gpu_ctx_bind(ctx, extra, extra_count);
        const SDL_GPUBufferBinding buffers[2] = {
            { .buffer = vertices },
            { .buffer = visible_buffer, .offset = batch->first * (uint32_t)sizeof(gpu_visible_t) },
        };
        SDL_BindGPUVertexBuffers(ctx->render, 0, buffers, 2);
        const GpuGeometryLod *lod = &geometry->lods[batch->lod];
        if (indices) {
            SDL_BindGPUIndexBuffer(ctx->render, &(SDL_GPUBufferBinding){ .buffer = indices },
                                   geometry->index_size == 4 ? SDL_GPU_INDEXELEMENTSIZE_32BIT
                                                             : SDL_GPU_INDEXELEMENTSIZE_16BIT);
            gpu_cmd_draw_indexed(ctx, lod->index_count, batch->count, lod->first_index,
                                 lod->vertex_offset, 0);
        } else {
            gpu_cmd_draw(ctx, lod->index_count, batch->count, lod->first_index, 0);
        }
    }
}

void gpu_batches_release(void) {
    if (visible_buffer) {
        gpu_uploads_drop_raw(visible_buffer);
        gpu_release_buffer(visible_buffer);
        gpu_stats()->memory_reserved -= visible_capacity * sizeof(gpu_visible_t);
    }
    visible_buffer = NULL;
    visible_capacity = 0;
    uploaded.count = 0;
    batches.count = 0;
}

void gpu_batches_fini(void) {
    gpu_vec_free(batches);
    gpu_vec_free(visible);
    gpu_vec_free(uploaded);
    gpu_vec_free(items);
    gpu_vec_free(groups);
    gpu_vec_free(counts);
    visible_buffer = NULL;
    visible_capacity = 0;
}
