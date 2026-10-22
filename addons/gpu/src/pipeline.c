#include "sigpu_internal.h"
#include <string.h>

static const struct {
    SDL_GPUVertexElementFormat sdl;
    uint32_t size;
} vertex_formats[GpuVertexFormatCount] = {
    [GpuVertexInvalid] = { SDL_GPU_VERTEXELEMENTFORMAT_INVALID, 0 },
    [GpuVertexFloat] = { SDL_GPU_VERTEXELEMENTFORMAT_FLOAT, 4 },
    [GpuVertexFloat2] = { SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 8 },
    [GpuVertexFloat3] = { SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 12 },
    [GpuVertexFloat4] = { SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 16 },
    [GpuVertexUint] = { SDL_GPU_VERTEXELEMENTFORMAT_UINT, 4 },
    [GpuVertexUint2] = { SDL_GPU_VERTEXELEMENTFORMAT_UINT2, 8 },
    [GpuVertexUint4] = { SDL_GPU_VERTEXELEMENTFORMAT_UINT4, 16 },
    [GpuVertexInt] = { SDL_GPU_VERTEXELEMENTFORMAT_INT, 4 },
    [GpuVertexUbyte4] = { SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4, 4 },
    [GpuVertexUbyte4Norm] = { SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, 4 },
    [GpuVertexByte4Norm] = { SDL_GPU_VERTEXELEMENTFORMAT_BYTE4_NORM, 4 },
    [GpuVertexShort2Norm] = { SDL_GPU_VERTEXELEMENTFORMAT_SHORT2_NORM, 4 },
    [GpuVertexShort4Norm] = { SDL_GPU_VERTEXELEMENTFORMAT_SHORT4_NORM, 8 },
    [GpuVertexHalf2] = { SDL_GPU_VERTEXELEMENTFORMAT_HALF2, 4 },
    [GpuVertexHalf4] = { SDL_GPU_VERTEXELEMENTFORMAT_HALF4, 8 },
};

static uint64_t fnv(uint64_t hash, const void *data, size_t size) {
    const uint8_t *bytes = data;
    for (size_t i = 0; i < size; i++) {
        hash = (hash ^ bytes[i]) * 0x100000001b3ull;
    }
    return hash;
}

/* Hashes the meaningful bytes only: names up to their terminator, unused
 * slots and padding ignored. */
uint64_t gpu_layout_hash(const GpuVertexLayout *layout) {
    uint64_t hash = 0xcbf29ce484222325ull;
    if (!layout) {
        return hash;
    }
    for (uint32_t i = 0; i < 2; i++) {
        hash = fnv(hash, &layout->strides[i], sizeof(uint32_t));
        hash = fnv(hash, &layout->instanced[i], sizeof(bool));
    }
    const uint32_t count = layout->attribute_count < GPU_LAYOUT_ATTRIBUTES
                               ? layout->attribute_count
                               : GPU_LAYOUT_ATTRIBUTES;
    for (uint32_t i = 0; i < count; i++) {
        const GpuVertexAttribute *attribute = &layout->attributes[i];
        hash = fnv(hash, attribute->name, SDL_strnlen(attribute->name, sizeof(attribute->name)));
        hash = fnv(hash, &attribute->format, 1);
        hash = fnv(hash, &attribute->buffer, 1);
        hash = fnv(hash, &attribute->offset, 2);
    }
    return hash;
}

uint32_t gpu_program_stage(const gpu_record_t *program, ecs_entity_t phase) {
    for (uint32_t i = 0; i < GPU_PROGRAM_STAGES; i++) {
        const gpu_program_stage_t *stage = &program->as.program.stages[i];
        if (stage->used && stage->phase == phase) {
            return i;
        }
    }
    return GPU_NONE;
}

static const GpuVertexAttribute *layout_attribute(const GpuVertexLayout *layout, const char *name) {
    for (uint32_t i = 0; layout && i < layout->attribute_count && i < GPU_LAYOUT_ATTRIBUTES; i++) {
        if (strncmp(layout->attributes[i].name, name, sizeof(layout->attributes[i].name)) == 0) {
            return &layout->attributes[i];
        }
    }
    return NULL;
}

static SDL_GPUColorTargetBlendState blend_state(uint8_t blend) {
    SDL_GPUColorTargetBlendState state = { .enable_blend = blend != GpuBlendNone,
                                           .color_blend_op = SDL_GPU_BLENDOP_ADD,
                                           .alpha_blend_op = SDL_GPU_BLENDOP_ADD };
    switch (blend) {
    case GpuBlendAlpha:
        state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        break;
    case GpuBlendAdditive:
        state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        break;
    case GpuBlendPremultiplied:
        state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        break;
    default:
        break;
    }
    return state;
}

static SDL_GPUCompareOp compare_op(uint8_t compare) {
    static const SDL_GPUCompareOp ops[] = {
        [GpuCompareNone] = SDL_GPU_COMPAREOP_ALWAYS,
        [GpuCompareNever] = SDL_GPU_COMPAREOP_NEVER,
        [GpuCompareLess] = SDL_GPU_COMPAREOP_LESS,
        [GpuCompareEqual] = SDL_GPU_COMPAREOP_EQUAL,
        [GpuCompareLessEqual] = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
        [GpuCompareGreater] = SDL_GPU_COMPAREOP_GREATER,
        [GpuCompareNotEqual] = SDL_GPU_COMPAREOP_NOT_EQUAL,
        [GpuCompareGreaterEqual] = SDL_GPU_COMPAREOP_GREATER_OR_EQUAL,
        [GpuCompareAlways] = SDL_GPU_COMPAREOP_ALWAYS,
    };
    return ops[compare <= GpuCompareAlways ? compare : GpuCompareNone];
}

static SDL_GPUPrimitiveType primitive(uint8_t topology) {
    switch (topology) {
    case GpuTopologyTriangleStrip:
        return SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
    case GpuTopologyLines:
        return SDL_GPU_PRIMITIVETYPE_LINELIST;
    case GpuTopologyLineStrip:
        return SDL_GPU_PRIMITIVETYPE_LINESTRIP;
    case GpuTopologyPoints:
        return SDL_GPU_PRIMITIVETYPE_POINTLIST;
    default:
        return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    }
}

static SDL_GPUGraphicsPipeline *create(const gpu_record_t *program, uint32_t stage_index,
                                       const gpu_targets_t *targets, const GpuVertexLayout *layout) {
    const gpu_program_stage_t *stage = &program->as.program.stages[stage_index];
    const gpu_pack_t *pack = gpu_program_pack(program);
    const gpu_pack_entry_t *vertex = gpu_pack_entry_at(pack, stage->vertex_entry);
    if (!vertex) {
        return NULL;
    }
    SDL_GPUVertexAttribute attributes[16];
    SDL_GPUVertexBufferDescription buffers[2];
    bool slot_used[2] = { false, false };
    uint32_t attribute_count = 0;
    for (uint32_t i = 0; i < vertex->input_count && attribute_count < 16; i++) {
        const gpu_pack_input_t *input = &pack->inputs[vertex->first_input + i];
        const GpuVertexAttribute *attribute = layout_attribute(layout, input->name);
        if (!attribute || attribute->format == GpuVertexInvalid ||
            attribute->format >= GpuVertexFormatCount || attribute->buffer > 1) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "sigpu: %s: vertex input '%s' has no attribute",
                         ecs_entity_name(program->entity), input->name);
            return NULL;
        }
        slot_used[attribute->buffer] = true;
        attributes[attribute_count++] = (SDL_GPUVertexAttribute){
            .location = input->location,
            .buffer_slot = attribute->buffer,
            .format = vertex_formats[attribute->format].sdl,
            .offset = attribute->offset,
        };
    }
    uint32_t buffer_count = 0;
    for (uint32_t slot = 0; slot < 2; slot++) {
        if (slot_used[slot]) {
            buffers[buffer_count++] = (SDL_GPUVertexBufferDescription){
                .slot = slot,
                .pitch = layout->strides[slot],
                .input_rate = layout->instanced[slot] ? SDL_GPU_VERTEXINPUTRATE_INSTANCE
                                                      : SDL_GPU_VERTEXINPUTRATE_VERTEX,
            };
        }
    }
    SDL_GPUColorTargetDescription colors[GPU_MAX_COLOR_TARGETS];
    for (uint32_t i = 0; i < targets->color_count; i++) {
        colors[i] = (SDL_GPUColorTargetDescription){
            .format = targets->colors[i],
            .blend_state = blend_state(stage->raster.blend),
        };
    }
    const bool depth = targets->depth != SDL_GPU_TEXTUREFORMAT_INVALID;
    const GpuRaster *raster = &stage->raster;
    SDL_GPUGraphicsPipelineCreateInfo info = {
        .vertex_shader = stage->vertex,
        .fragment_shader = stage->fragment,
        .vertex_input_state = { .vertex_buffer_descriptions = buffers,
                                .num_vertex_buffers = buffer_count,
                                .vertex_attributes = attributes,
                                .num_vertex_attributes = attribute_count },
        .primitive_type = primitive(raster->topology),
        .rasterizer_state = {
            .fill_mode = SDL_GPU_FILLMODE_FILL,
            .cull_mode = raster->cull == GpuFaceBack    ? SDL_GPU_CULLMODE_BACK
                         : raster->cull == GpuFaceFront ? SDL_GPU_CULLMODE_FRONT
                                                         : SDL_GPU_CULLMODE_NONE,
            .front_face = raster->front_clockwise ? SDL_GPU_FRONTFACE_CLOCKWISE
                                                  : SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
            .depth_bias_constant_factor = raster->depth_bias,
            .depth_bias_slope_factor = raster->slope_bias,
            .enable_depth_bias = raster->depth_bias != 0.0f || raster->slope_bias != 0.0f,
        },
        .multisample_state = { .sample_count = gpu_sample_count(targets->samples) },
        .depth_stencil_state = {
            .compare_op = compare_op(raster->depth_compare),
            .enable_depth_test = depth && raster->depth_compare != GpuCompareNone,
            .enable_depth_write = depth && raster->depth_write,
        },
        .target_info = { .color_target_descriptions = colors,
                         .num_color_targets = targets->color_count,
                         .depth_stencil_format = targets->depth,
                         .has_depth_stencil_target = depth },
    };
    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(gpu_session.gpu, &info);
    gpu_counted_create(pipeline, "graphics pipeline");
    return pipeline;
}

static bool key_equal(const gpu_pipeline_key_t *a, const gpu_pipeline_key_t *b) {
    if (a->record != b->record || a->generation != b->generation || a->stage != b->stage ||
        a->layout != b->layout || a->targets.color_count != b->targets.color_count ||
        a->targets.depth != b->targets.depth || a->targets.samples != b->targets.samples) {
        return false;
    }
    for (uint32_t i = 0; i < a->targets.color_count; i++) {
        if (a->targets.colors[i] != b->targets.colors[i]) {
            return false;
        }
    }
    return true;
}

SDL_GPUGraphicsPipeline *gpu_pipeline_get(
    const gpu_record_t *program,
    uint32_t stage,
    const gpu_targets_t *targets,
    const GpuVertexLayout *layout,
    bool allow_create
) {
    if (!program || program->kind != GPU_RESOURCE_PROGRAM || stage >= GPU_PROGRAM_STAGES ||
        !program->as.program.stages[stage].used) {
        return NULL;
    }
    const gpu_pipeline_key_t key = {
        .record = (uint32_t)(program - gpu_session.records.items) + 1,
        .generation = program->generation,
        .stage = stage,
        .targets = *targets,
        .layout = gpu_layout_hash(layout),
    };
    GpuStats *stats = gpu_stats();
    for (uint32_t i = 0; i < gpu_session.pipelines.count; i++) {
        gpu_pipeline_t *cached = &gpu_session.pipelines.items[i];
        if (key_equal(&cached->key, &key)) {
            stats->pipeline_hits++;
            return cached->pipeline;
        }
    }
    stats->pipeline_misses++;
    if (!allow_create ||
        (!gpu_session.prewarm && stats->pipelines_created >= gpu_session.pipeline_budget)) {
        return NULL;
    }
    stats->pipelines_created++;
    gpu_pipeline_t entry = { .key = key, .pipeline = create(program, stage, targets, layout) };
    entry.failed = entry.pipeline == NULL;
    gpu_vec_push(gpu_session.pipelines, entry);
    return entry.pipeline;
}

static void pipeline_release(gpu_pipeline_t *entry) {
    if (entry->pipeline) {
        gpu_counted_release(entry->pipeline);
        SDL_ReleaseGPUGraphicsPipeline(gpu_session.gpu, entry->pipeline);
        entry->pipeline = NULL;
    }
}

void gpu_pipelines_forget(uint32_t record) {
    uint32_t kept = 0;
    for (uint32_t i = 0; i < gpu_session.pipelines.count; i++) {
        gpu_pipeline_t *entry = &gpu_session.pipelines.items[i];
        if (entry->key.record == record) {
            pipeline_release(entry);
        } else {
            gpu_session.pipelines.items[kept++] = *entry;
        }
    }
    gpu_session.pipelines.count = kept;
}

void gpu_pipelines_release(void) {
    for (uint32_t i = 0; i < gpu_session.pipelines.count; i++) {
        pipeline_release(&gpu_session.pipelines.items[i]);
    }
    gpu_session.pipelines.count = 0;
}

static bool name_terminated(const char *name, size_t size) { return memchr(name, 0, size) != NULL; }

void gpu_geometry_build(ecs_entity_t entity, gpu_result_t *result) {
    const GpuGeometry *desc = ecs_get(entity, GpuGeometry);
    if (!desc->stride || desc->attribute_count > GPU_GEOMETRY_ATTRIBUTES) {
        gpu_fail(result, GpuErrorInvalid, 0, "stride must be set and attributes at most 8");
        return;
    }
    if (desc->index_size != 0 && desc->index_size != 2 && desc->index_size != 4) {
        gpu_fail(result, GpuErrorInvalid, 0, "index_size must be 0, 2 or 4");
        return;
    }
    if (!desc->lod_count || desc->lod_count > GPU_GEOMETRY_LODS ||
        desc->topology > GpuTopologyPoints) {
        gpu_fail(result, GpuErrorInvalid, 0, "lod_count must be 1 to 4 with a GpuTopology");
        return;
    }
    for (uint32_t i = 0; i < desc->attribute_count; i++) {
        const GpuVertexAttribute *attribute = &desc->attributes[i];
        if (!name_terminated(attribute->name, sizeof(attribute->name)) || !attribute->name[0] ||
            attribute->format == GpuVertexInvalid || attribute->format >= GpuVertexFormatCount ||
            attribute->buffer != 0 ||
            attribute->offset + vertex_formats[attribute->format].size > desc->stride) {
            gpu_fail(result, GpuErrorInvalid, 0, "attribute %u is invalid or outside the stride",
                     i);
            return;
        }
    }
    gpu_edge_add(desc->vertices, entity);
    if (desc->index_size) {
        gpu_edge_add(desc->indices, entity);
    }
    if (!gpu_dependency_ready(result, desc->vertices, GPU_RESOURCE_BUFFER, "vertices")) {
        return;
    }
    if (desc->index_size &&
        !gpu_dependency_ready(result, desc->indices, GPU_RESOURCE_BUFFER, "indices")) {
        return;
    }
    result->built.as.geometry.vertices = desc->vertices;
    result->built.as.geometry.indices = desc->index_size ? desc->indices : 0;
    result->status = GpuReady;
    result->built.kind = GPU_RESOURCE_GEOMETRY;
}
