#include "sigpu_internal.h"
#include <string.h>

/* ----------------------------------------------------------------------
 * Bindings
 * ---------------------------------------------------------------------- */

static const gpu_ctx_resource_t *named_resource(const gpu_pass_ctx_t *ctx, const char *name) {
    for (uint32_t i = 0; i < ctx->resource_count; i++) {
        if (strcmp(ctx->resources[i].name, name) == 0) {
            return &ctx->resources[i];
        }
    }
    return NULL;
}

static const gpu_ctx_uniform_t *named_uniform(const gpu_pass_ctx_t *ctx, const char *name) {
    for (uint32_t i = 0; i < ctx->uniform_count; i++) {
        if (strcmp(ctx->uniforms[i].name, name) == 0) {
            return &ctx->uniforms[i];
        }
    }
    return NULL;
}

static const GpuAccess *pass_binding(const gpu_pass_ctx_t *ctx, const char *name) {
    const GpuPassIO *io = ctx->pass ? &ctx->pass->io : NULL;
    for (uint32_t i = 0; io && i < io->count; i++) {
        if (io->items[i].binding[0] && strncmp(io->items[i].binding, name, 32) == 0) {
            return &io->items[i];
        }
    }
    return NULL;
}

static SDL_GPUSampler *sampler_of(ecs_entity_t sampler) {
    const gpu_record_t *record = sampler ? gpu_resource_record(sampler, GPU_RESOURCE_SAMPLER)
                                         : NULL;
    return record ? record->as.sampler : gpu_default_sampler();
}

/* SDL objects for an entity binding. */
static bool resolve(ecs_entity_t resource, ecs_entity_t sampler, gpu_binding_t *out) {
    const gpu_record_t *record = gpu_resource_record(resource, GPU_RESOURCE_TEXTURE);
    if (record) {
        out->texture = record->as.texture.texture;
        out->sampler = sampler_of(sampler);
        return true;
    }
    record = gpu_resource_record(resource, GPU_RESOURCE_BUFFER);
    if (record) {
        out->buffer = record->as.buffer.buffer;
        return true;
    }
    return false;
}

static uint32_t warnings;

static void unbound(const gpu_pass_ctx_t *ctx, const char *name) {
    if (warnings < 32) {
        warnings++;
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "sigpu: pass %s: nothing bound to '%s'",
                    ctx->pass ? ecs_entity_name(ctx->pass->entity) : "?", name);
    }
}

static void bind_one(gpu_pass_ctx_t *ctx, uint32_t stage, const gpu_pack_resource_t *resource,
                     const gpu_binding_t *binding) {
    const uint32_t slot = resource->slot;
    switch (resource->kind) {
    case GpuPackSampler: {
        if (!binding->texture || !binding->sampler) {
            unbound(ctx, resource->name);
            return;
        }
        const SDL_GPUTextureSamplerBinding bound = { binding->texture, binding->sampler };
        if (ctx->compute) {
            SDL_BindGPUComputeSamplers(ctx->compute, slot, &bound, 1);
        } else if (stage == GpuPackVertex) {
            SDL_BindGPUVertexSamplers(ctx->render, slot, &bound, 1);
        } else {
            SDL_BindGPUFragmentSamplers(ctx->render, slot, &bound, 1);
        }
        return;
    }
    case GpuPackStorageTexture:
        if (!binding->texture) {
            unbound(ctx, resource->name);
            return;
        }
        if (ctx->compute) {
            SDL_BindGPUComputeStorageTextures(ctx->compute, slot, &binding->texture, 1);
        } else if (stage == GpuPackVertex) {
            SDL_BindGPUVertexStorageTextures(ctx->render, slot, &binding->texture, 1);
        } else {
            SDL_BindGPUFragmentStorageTextures(ctx->render, slot, &binding->texture, 1);
        }
        return;
    case GpuPackStorageBuffer:
        if (!binding->buffer) {
            unbound(ctx, resource->name);
            return;
        }
        if (ctx->compute) {
            SDL_BindGPUComputeStorageBuffers(ctx->compute, slot, &binding->buffer, 1);
        } else if (stage == GpuPackVertex) {
            SDL_BindGPUVertexStorageBuffers(ctx->render, slot, &binding->buffer, 1);
        } else {
            SDL_BindGPUFragmentStorageBuffers(ctx->render, slot, &binding->buffer, 1);
        }
        return;
    default:
        return;
    }
}

static void push_uniform(gpu_pass_ctx_t *ctx, uint32_t stage, uint32_t slot, const void *data,
                         uint32_t size) {
    if (ctx->compute) {
        SDL_PushGPUComputeUniformData(ctx->cmd, slot, data, size);
    } else if (stage == GpuPackVertex) {
        SDL_PushGPUVertexUniformData(ctx->cmd, slot, data, size);
    } else {
        SDL_PushGPUFragmentUniformData(ctx->cmd, slot, data, size);
    }
}

void gpu_ctx_bind(gpu_pass_ctx_t *ctx, const gpu_binding_t *extra, uint32_t extra_count) {
    for (uint32_t e = 0; e < 2; e++) {
        const gpu_pack_entry_t *entry = ctx->entries[e];
        if (!entry) {
            continue;
        }
        for (uint32_t r = 0; r < entry->resource_count; r++) {
            const gpu_pack_resource_t *resource = &ctx->pack->resources[entry->first_resource + r];
            if (resource->kind == GpuPackRWStorageBuffer ||
                resource->kind == GpuPackRWStorageTexture) {
                continue;
            }
            if (resource->kind == GpuPackUniformBuffer) {
                const gpu_ctx_uniform_t *uniform = named_uniform(ctx, resource->name);
                if (uniform) {
                    push_uniform(ctx, entry->stage, resource->slot,
                                 ctx->uniform_data + uniform->offset, uniform->size);
                } else {
                    bool found = false;
                    for (uint32_t x = 0; x < extra_count && !found; x++) {
                        found = extra[x].name && strcmp(extra[x].name, resource->name) == 0;
                    }
                    if (!found) {
                        unbound(ctx, resource->name);
                    }
                }
                continue;
            }
            gpu_binding_t binding = { 0 };
            bool found = false;
            for (uint32_t x = 0; x < extra_count && !found; x++) {
                if (extra[x].name ? strcmp(extra[x].name, resource->name) == 0
                                  : extra[x].scope && extra[x].scope == resource->scope &&
                                        (resource->kind == GpuPackStorageBuffer) ==
                                            (extra[x].buffer != NULL)) {
                    binding = extra[x];
                    found = true;
                }
            }
            if (!found) {
                const gpu_ctx_resource_t *named = named_resource(ctx, resource->name);
                const GpuAccess *access = named ? NULL : pass_binding(ctx, resource->name);
                if (named) {
                    found = resolve(named->resource, named->sampler, &binding);
                } else if (access) {
                    found = resolve(access->resource, access->aux, &binding);
                }
            }
            bind_one(ctx, entry->stage, resource, &binding);
        }
    }
}

bool gpu_ctx_pipeline(gpu_pass_ctx_t *ctx, const gpu_record_t *program, uint32_t stage,
                      SDL_GPUGraphicsPipeline *pipeline) {
    if (!ctx->render || !pipeline) {
        return false;
    }
    if (pipeline != ctx->pipeline) {
        SDL_BindGPUGraphicsPipeline(ctx->render, pipeline);
        gpu_stats()->pipeline_binds++;
        ctx->pipeline = pipeline;
    }
    const gpu_pack_t *pack = gpu_program_pack(program);
    const gpu_program_stage_t *entries = &program->as.program.stages[stage];
    ctx->program = program;
    ctx->stage = stage;
    ctx->pack = pack;
    ctx->entries[0] = gpu_pack_entry_at(pack, entries->vertex_entry);
    ctx->entries[1] = gpu_pack_entry_at(pack, entries->fragment_entry);
    return ctx->entries[0] != NULL;
}

/* ----------------------------------------------------------------------
 * Commands
 * ---------------------------------------------------------------------- */

void gpu_pass_extent(const gpu_pass_ctx_t *ctx, uint32_t *width, uint32_t *height) {
    if (width) {
        *width = ctx ? ctx->width : 0;
    }
    if (height) {
        *height = ctx ? ctx->height : 0;
    }
}

uint64_t gpu_pass_frame(const gpu_pass_ctx_t *ctx) {
    (void)ctx;
    return gpu_session.frame;
}

bool gpu_cmd_program(gpu_pass_ctx_t *ctx, ecs_entity_t program, const GpuVertexLayout *layout) {
    const gpu_record_t *record = gpu_resource_record(program, GPU_RESOURCE_PROGRAM);
    if (!ctx || !record) {
        return false;
    }
    if (ctx->compute) {
        if (!record->as.program.compute) {
            return false;
        }
        SDL_BindGPUComputePipeline(ctx->compute, record->as.program.compute);
        ctx->program = record;
        ctx->pack = gpu_program_pack(record);
        ctx->entries[0] = gpu_pack_entry_at(ctx->pack, record->as.program.compute_entry);
        ctx->entries[1] = NULL;
        gpu_ctx_bind(ctx, NULL, 0);
        return true;
    }
    if (!ctx->render) {
        return false;
    }
    uint32_t stage = ctx->pass ? gpu_program_stage(record, ctx->pass->desc.phase) : GPU_NONE;
    if (stage == GPU_NONE) {
        stage = 0;
    }
    SDL_GPUGraphicsPipeline *pipeline =
        gpu_pipeline_get(record, stage, &ctx->targets, layout, true);
    if (!gpu_ctx_pipeline(ctx, record, stage, pipeline)) {
        return false;
    }
    gpu_ctx_bind(ctx, NULL, 0);
    return true;
}

static void copy_name(char *out, const char *name) {
    const size_t length = SDL_strnlen(name, GPU_PACK_NAME_SIZE - 1);
    memcpy(out, name, length);
    out[length] = '\0';
}

void gpu_cmd_uniform(gpu_pass_ctx_t *ctx, const char *name, const void *data, uint32_t size) {
    if (!ctx || !name || !data || !size) {
        return;
    }
    gpu_ctx_uniform_t *uniform = (gpu_ctx_uniform_t *)named_uniform(ctx, name);
    if (!uniform || uniform->size < size) {
        if (ctx->uniform_used + size > GPU_CTX_UNIFORM_BYTES ||
            (!uniform && ctx->uniform_count == GPU_CTX_BINDINGS)) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "sigpu: too many uniforms in one pass");
            return;
        }
        if (!uniform) {
            uniform = &ctx->uniforms[ctx->uniform_count++];
            copy_name(uniform->name, name);
        }
        uniform->offset = ctx->uniform_used;
        ctx->uniform_used += (size + 15u) & ~15u;
    }
    uniform->size = size;
    memcpy(ctx->uniform_data + uniform->offset, data, size);
    for (uint32_t e = 0; e < 2 && ctx->pack; e++) {
        const gpu_pack_entry_t *entry = ctx->entries[e];
        const gpu_pack_resource_t *resource =
            entry ? gpu_pack_resource(ctx->pack, entry, name) : NULL;
        if (resource && resource->kind == GpuPackUniformBuffer) {
            push_uniform(ctx, entry->stage, resource->slot, data, size);
        }
    }
}

static void set_resource(gpu_pass_ctx_t *ctx, const char *name, ecs_entity_t resource,
                         ecs_entity_t sampler) {
    if (!ctx || !name) {
        return;
    }
    gpu_ctx_resource_t *entry = (gpu_ctx_resource_t *)named_resource(ctx, name);
    if (!entry) {
        if (ctx->resource_count == GPU_CTX_BINDINGS) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "sigpu: too many bindings in one pass");
            return;
        }
        entry = &ctx->resources[ctx->resource_count++];
        copy_name(entry->name, name);
    }
    entry->resource = resource;
    entry->sampler = sampler;
    for (uint32_t e = 0; e < 2 && ctx->pack; e++) {
        const gpu_pack_entry_t *pack_entry = ctx->entries[e];
        const gpu_pack_resource_t *bound =
            pack_entry ? gpu_pack_resource(ctx->pack, pack_entry, name) : NULL;
        gpu_binding_t binding = { 0 };
        if (bound && resolve(resource, sampler, &binding)) {
            bind_one(ctx, pack_entry->stage, bound, &binding);
        }
    }
}

void gpu_cmd_texture(gpu_pass_ctx_t *ctx, const char *name, ecs_entity_t texture,
                     ecs_entity_t sampler) {
    set_resource(ctx, name, texture, sampler);
}

void gpu_cmd_storage(gpu_pass_ctx_t *ctx, const char *name, ecs_entity_t resource) {
    set_resource(ctx, name, resource, 0);
}

void gpu_cmd_vertex_buffer(gpu_pass_ctx_t *ctx, uint32_t slot, ecs_entity_t buffer,
                           uint32_t offset) {
    SDL_GPUBuffer *sdl = gpu_resource_buffer(buffer);
    if (ctx && ctx->render && sdl) {
        SDL_BindGPUVertexBuffers(ctx->render, slot,
                                 &(SDL_GPUBufferBinding){ .buffer = sdl, .offset = offset }, 1);
    }
}

void gpu_cmd_index_buffer(gpu_pass_ctx_t *ctx, ecs_entity_t buffer, uint32_t offset,
                          uint8_t index_size) {
    SDL_GPUBuffer *sdl = gpu_resource_buffer(buffer);
    if (ctx && ctx->render && sdl) {
        SDL_BindGPUIndexBuffer(ctx->render,
                               &(SDL_GPUBufferBinding){ .buffer = sdl, .offset = offset },
                               index_size == 4 ? SDL_GPU_INDEXELEMENTSIZE_32BIT
                                               : SDL_GPU_INDEXELEMENTSIZE_16BIT);
    }
}

void gpu_cmd_viewport(gpu_pass_ctx_t *ctx, float x, float y, float w, float h) {
    if (ctx && ctx->render) {
        SDL_SetGPUViewport(ctx->render, &(SDL_GPUViewport){ x, y, w, h, 0.0f, 1.0f });
    }
}

void gpu_cmd_scissor(gpu_pass_ctx_t *ctx, int32_t x, int32_t y, int32_t w, int32_t h) {
    if (ctx && ctx->render) {
        SDL_SetGPUScissor(ctx->render, &(SDL_Rect){ x, y, w, h });
    }
}

void gpu_cmd_draw(gpu_pass_ctx_t *ctx, uint32_t vertex_count, uint32_t instance_count,
                  uint32_t first_vertex, uint32_t first_instance) {
    if (ctx && ctx->render && ctx->pipeline) {
        SDL_DrawGPUPrimitives(ctx->render, vertex_count, instance_count, first_vertex,
                              first_instance);
        gpu_stats()->draws++;
    }
}

void gpu_cmd_draw_indexed(gpu_pass_ctx_t *ctx, uint32_t index_count, uint32_t instance_count,
                          uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) {
    if (ctx && ctx->render && ctx->pipeline) {
        SDL_DrawGPUIndexedPrimitives(ctx->render, index_count, instance_count, first_index,
                                     vertex_offset, first_instance);
        gpu_stats()->draws++;
    }
}

void gpu_cmd_dispatch(gpu_pass_ctx_t *ctx, uint32_t x, uint32_t y, uint32_t z) {
    if (ctx && ctx->compute && ctx->program && x && y && z) {
        SDL_DispatchGPUCompute(ctx->compute, x, y, z);
    }
}

void gpu_cmd_dispatch_cover(gpu_pass_ctx_t *ctx, uint32_t width, uint32_t height) {
    const gpu_pack_entry_t *entry = ctx ? ctx->entries[0] : NULL;
    if (!entry || entry->stage != GpuPackCompute) {
        return;
    }
    const uint32_t tx = entry->threads[0] ? entry->threads[0] : 1;
    const uint32_t ty = entry->threads[1] ? entry->threads[1] : 1;
    gpu_cmd_dispatch(ctx, (width + tx - 1) / tx, (height + ty - 1) / ty, 1);
}

void gpu_cmd_copy_buffer(gpu_pass_ctx_t *ctx, ecs_entity_t source, uint32_t source_offset,
                         ecs_entity_t destination, uint32_t destination_offset, uint32_t size) {
    SDL_GPUBuffer *from = gpu_resource_buffer(source);
    SDL_GPUBuffer *to = gpu_resource_buffer(destination);
    if (ctx && ctx->copy && from && to && size) {
        SDL_CopyGPUBufferToBuffer(
            ctx->copy, &(SDL_GPUBufferLocation){ .buffer = from, .offset = source_offset },
            &(SDL_GPUBufferLocation){ .buffer = to, .offset = destination_offset }, size, false);
    }
}

void gpu_cmd_copy_texture(gpu_pass_ctx_t *ctx, ecs_entity_t source, ecs_entity_t destination) {
    gpu_texture_info_t from_info, to_info;
    SDL_GPUTexture *from = gpu_resource_texture(source, &from_info);
    SDL_GPUTexture *to = gpu_resource_texture(destination, &to_info);
    if (ctx && ctx->copy && from && to && from_info.width == to_info.width &&
        from_info.height == to_info.height) {
        SDL_CopyGPUTextureToTexture(ctx->copy, &(SDL_GPUTextureLocation){ .texture = from },
                                    &(SDL_GPUTextureLocation){ .texture = to }, from_info.width,
                                    from_info.height, 1, false);
    }
}

/* ----------------------------------------------------------------------
 * Pass recording
 * ---------------------------------------------------------------------- */

static SDL_GPULoadOp load_op(uint8_t load) {
    return load == GpuLoadLoad       ? SDL_GPU_LOADOP_LOAD
           : load == GpuLoadDontCare ? SDL_GPU_LOADOP_DONT_CARE
                                     : SDL_GPU_LOADOP_CLEAR;
}

static uint32_t mip_extent(uint32_t size, uint32_t mip) {
    const uint32_t scaled = mip < 32 ? size >> mip : 0;
    return scaled ? scaled : 1;
}

/* SDL texture, format and size of an attachment; NULL when unavailable. */
static SDL_GPUTexture *attachment(SDL_GPUCommandBuffer *cmd, const GpuAccess *access,
                                  SDL_GPUTextureFormat *format, uint32_t *width,
                                  uint32_t *height) {
    if (gpu_resource_kind(access->resource) == GPU_RESOURCE_SURFACE) {
        const gpu_record_t *record = gpu_resource_record(access->resource, GPU_RESOURCE_SURFACE);
        if (!record) {
            return NULL;
        }
        *format = record->as.surface.format;
        return gpu_surface_acquire(cmd, access->resource, width, height);
    }
    gpu_texture_info_t info;
    SDL_GPUTexture *texture = gpu_resource_texture(access->resource, &info);
    *format = gpu_sdl_format(info.format);
    *width = mip_extent(info.width, access->mip);
    *height = mip_extent(info.height, access->mip);
    return texture;
}

static void call_back(gpu_pass_ctx_t *ctx) {
    gpu_callback_t *callback = gpu_callback_of(ctx->pass->entity);
    if (callback) {
        callback->fn(ctx, ctx->pass->entity, callback->user_data);
    }
}

static void viewport(gpu_pass_ctx_t *ctx) {
    const float *v = ctx->pass->desc.viewport;
    if (v[2] > 0.0f && v[3] > 0.0f) {
        gpu_cmd_viewport(ctx, v[0] * ctx->width, v[1] * ctx->height, v[2] * ctx->width,
                         v[3] * ctx->height);
    }
}

static bool record_render(gpu_pass_ctx_t *ctx) {
    gpu_plan_pass_t *pass = ctx->pass;
    SDL_GPUColorTargetInfo colors[GPU_MAX_COLOR_TARGETS];
    SDL_GPUDepthStencilTargetInfo depth = { 0 };
    bool has_depth = false;
    ctx->targets = (gpu_targets_t){ .depth = SDL_GPU_TEXTUREFORMAT_INVALID, .samples = 1 };
    for (uint32_t i = 0; i < pass->io.count; i++) {
        const GpuAccess *access = &pass->io.items[i];
        if (access->usage != GpuUseColor && access->usage != GpuUseDepth) {
            continue;
        }
        SDL_GPUTextureFormat format;
        uint32_t width = 0, height = 0;
        SDL_GPUTexture *texture = attachment(ctx->cmd, access, &format, &width, &height);
        if (!texture) {
            return false;
        }
        if (!ctx->width) {
            ctx->width = width;
            ctx->height = height;
        } else if (width != ctx->width || height != ctx->height) {
            /* Attachments of one pass share their size (RFC section 8.3). */
            if (warnings < 32) {
                warnings++;
                SDL_LogError(SDL_LOG_CATEGORY_GPU, "sigpu: pass %s: attachments differ in size",
                             ecs_entity_name(pass->entity));
            }
            return false;
        }
        gpu_texture_info_t info = { .samples = 1 };
        gpu_resource_texture(access->resource, &info);
        ctx->targets.samples = info.samples ? info.samples : 1;
        if (access->usage == GpuUseDepth) {
            has_depth = true;
            ctx->targets.depth = format;
            depth = (SDL_GPUDepthStencilTargetInfo){
                .texture = texture,
                .clear_depth = access->clear[0],
                .load_op = load_op(access->load),
                .store_op = access->store == GpuStoreDontCare ? SDL_GPU_STOREOP_DONT_CARE
                                                              : SDL_GPU_STOREOP_STORE,
                .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
                .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
                .layer = (Uint8)access->layer,
                .mip_level = access->mip,
            };
            continue;
        }
        SDL_GPUTexture *resolve = access->aux ? gpu_resource_texture(access->aux, NULL) : NULL;
        if (access->aux && !resolve) {
            return false;
        }
        const uint32_t index = ctx->targets.color_count++;
        ctx->targets.colors[index] = format;
        colors[index] = (SDL_GPUColorTargetInfo){
            .texture = texture,
            .mip_level = access->mip,
            .layer_or_depth_plane = access->layer,
            .clear_color = { access->clear[0], access->clear[1], access->clear[2],
                             access->clear[3] },
            .load_op = load_op(access->load),
            .store_op = resolve ? (access->store == GpuStoreDontCare
                                       ? SDL_GPU_STOREOP_RESOLVE
                                       : SDL_GPU_STOREOP_RESOLVE_AND_STORE)
                        : access->store == GpuStoreDontCare ? SDL_GPU_STOREOP_DONT_CARE
                                                            : SDL_GPU_STOREOP_STORE,
            .resolve_texture = resolve,
        };
    }
    ctx->render = SDL_BeginGPURenderPass(ctx->cmd, colors, ctx->targets.color_count,
                                         has_depth ? &depth : NULL);
    if (!ctx->render) {
        return false;
    }
    viewport(ctx);
    if (pass->desc.program) {
        gpu_cmd_program(ctx, pass->desc.program, NULL);
    }
    if (pass->desc.kind == GpuPassPresent) {
        const gpu_record_t *surface =
            gpu_resource_record(pass->io.items[1].resource, GPU_RESOURCE_SURFACE);
        const uint32_t params[4] = { surface && !surface->as.surface.srgb, 0, 0, 0 };
        gpu_cmd_uniform(ctx, "present_params", params, sizeof(params));
        gpu_cmd_draw(ctx, 3, 1, 0, 0);
    } else {
        call_back(ctx);
        if (pass->batch_count) {
            gpu_draw_batches(ctx, pass);
        }
    }
    SDL_EndGPURenderPass(ctx->render);
    ctx->render = NULL;
    return true;
}

static bool record_compute(gpu_pass_ctx_t *ctx) {
    gpu_plan_pass_t *pass = ctx->pass;
    SDL_GPUStorageTextureReadWriteBinding textures[8];
    SDL_GPUStorageBufferReadWriteBinding buffers[8];
    uint32_t texture_count = 0, buffer_count = 0;
    const gpu_record_t *program = pass->desc.program
                                      ? gpu_resource_record(pass->desc.program,
                                                            GPU_RESOURCE_PROGRAM)
                                      : NULL;
    const gpu_pack_t *pack = gpu_program_pack(program);
    const gpu_pack_entry_t *entry =
        program ? gpu_pack_entry_at(pack, program->as.program.compute_entry) : NULL;
    /* Read-write resources bind at the start of the pass, in the slot order
     * of the program when it is known. */
    const uint32_t ordered = entry ? entry->resource_count : 0;
    for (uint32_t r = 0; r < (entry ? ordered : pass->io.count); r++) {
        const GpuAccess *access = NULL;
        uint32_t kind = 0;
        if (entry) {
            const gpu_pack_resource_t *resource = &pack->resources[entry->first_resource + r];
            if (resource->kind != GpuPackRWStorageBuffer &&
                resource->kind != GpuPackRWStorageTexture) {
                continue;
            }
            access = pass_binding(ctx, resource->name);
            kind = resource->kind;
            if (!access) {
                unbound(ctx, resource->name);
                return false;
            }
        } else {
            access = &pass->io.items[r];
            if (access->usage != GpuUseStorageWrite && access->usage != GpuUseStorageReadWrite) {
                continue;
            }
            kind = gpu_resource_kind(access->resource) == GPU_RESOURCE_BUFFER
                       ? GpuPackRWStorageBuffer
                       : GpuPackRWStorageTexture;
        }
        if (kind == GpuPackRWStorageBuffer) {
            SDL_GPUBuffer *buffer = gpu_resource_buffer(access->resource);
            if (!buffer || buffer_count == 8) {
                return false;
            }
            buffers[buffer_count++] = (SDL_GPUStorageBufferReadWriteBinding){ .buffer = buffer };
        } else {
            SDL_GPUTexture *texture = gpu_resource_texture(access->resource, NULL);
            if (!texture || texture_count == 8) {
                return false;
            }
            textures[texture_count++] = (SDL_GPUStorageTextureReadWriteBinding){
                .texture = texture, .mip_level = access->mip, .layer = access->layer };
        }
    }
    ctx->compute = SDL_BeginGPUComputePass(ctx->cmd, textures, texture_count, buffers,
                                           buffer_count);
    if (!ctx->compute) {
        return false;
    }
    if (pass->desc.program) {
        gpu_cmd_program(ctx, pass->desc.program, NULL);
    }
    call_back(ctx);
    SDL_EndGPUComputePass(ctx->compute);
    ctx->compute = NULL;
    return true;
}

static bool record_copy(gpu_pass_ctx_t *ctx) {
    ctx->copy = SDL_BeginGPUCopyPass(ctx->cmd);
    if (!ctx->copy) {
        return false;
    }
    call_back(ctx);
    SDL_EndGPUCopyPass(ctx->copy);
    ctx->copy = NULL;
    return true;
}

static gpu_pass_ctx_t pass_ctx;

static void record_pass(SDL_GPUCommandBuffer *cmd, gpu_plan_pass_t *pass) {
    gpu_pass_ctx_t *ctx = &pass_ctx;
    memset(ctx, 0, offsetof(gpu_pass_ctx_t, uniform_data));
    ctx->uniform_used = 0;
    ctx->resource_count = 0;
    ctx->cmd = cmd;
    ctx->pass = pass;
    bool recorded = false;
    switch (pass->desc.kind) {
    case GpuPassRender:
    case GpuPassPresent:
        recorded = record_render(ctx);
        break;
    case GpuPassCompute:
        recorded = record_compute(ctx);
        break;
    case GpuPassCopy:
        recorded = record_copy(ctx);
        break;
    default:
        break;
    }
    GpuStats *stats = gpu_stats();
    if (recorded) {
        stats->passes++;
    } else {
        stats->passes_suspended++;
        pass->cache_valid = false;
    }
}

void gpu_graphs_record(SDL_GPUCommandBuffer *cmd) {
    for (uint32_t g = 0; g < gpu_session.graphs.count; g++) {
        gpu_graph_t *graph = &gpu_session.graphs.items[g];
        if (!graph->valid) {
            continue;
        }
        uint64_t active = 1ull << 63;
        for (uint32_t r = 0; r < graph->root_count; r++) {
            uint32_t width, height;
            if (gpu_surface_acquire(cmd, graph->roots[r], &width, &height)) {
                active |= 1ull << r;
            }
        }
        for (uint32_t p = 0; p < graph->plan.count; p++) {
            gpu_plan_pass_t *pass = &graph->plan.items[p];
            if (!(pass->roots & active)) {
                gpu_stats()->passes_suspended++;
                pass->cache_valid = false;
                continue;
            }
            if (pass->skip) {
                gpu_stats()->passes_reused++;
                continue;
            }
            record_pass(cmd, pass);
        }
    }
}
