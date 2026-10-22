#include "sigpu_internal.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GPU_BUFFER_USAGE_ALL 0x3fu
#define GPU_TEXTURE_USAGE_ALL 0x3fu
#define GPU_SIZE_FROM_DEPTH 64

gpu_session_t gpu_session;

bool gpu_vec_reserve(void **items, uint32_t *capacity, uint32_t needed, size_t size) {
    if (needed <= *capacity) {
        return true;
    }
    uint32_t next = *capacity ? *capacity : 16;
    while (next < needed) {
        next *= 2;
    }
    void *grown = realloc(*items, (size_t)next * size);
    if (!grown) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "sigpu: out of memory");
        return false;
    }
    *items = grown;
    *capacity = next;
    return true;
}

GpuResourceState *gpu_state(ecs_entity_t entity) {
    return gpu_session.open && ecs_is_alive(entity) ? ecs_try_get(entity, GpuResourceState) : NULL;
}

gpu_record_t *gpu_record_at(uint32_t id) {
    gpu_record_t *record =
        id && id <= gpu_session.records.count ? &gpu_session.records.items[id - 1] : NULL;
    return record && record->kind != GPU_RESOURCE_NONE ? record : NULL;
}

static void record_release(uint32_t id) {
    gpu_record_t *record = gpu_record_at(id);
    if (record) {
        if (record->kind == GPU_RESOURCE_PROGRAM) {
            gpu_pipelines_forget(id);
        }
        gpu_record_release(record);
        gpu_vec_push(gpu_session.free_records, id);
    }
}

static void release_doomed(void) {
    for (uint32_t i = 0; i < gpu_session.doomed.count; i++) {
        record_release(gpu_session.doomed.items[i]);
    }
    gpu_session.doomed.count = 0;
}

void gpu_doom(uint32_t record) {
    if (gpu_record_at(record)) {
        gpu_vec_push(gpu_session.doomed, record);
    }
}

static uint32_t record_store(const gpu_record_t *built) {
    if (gpu_session.free_records.count) {
        const uint32_t id = gpu_session.free_records.items[--gpu_session.free_records.count];
        gpu_record_t *slot = &gpu_session.records.items[id - 1];
        const uint32_t generation = slot->generation + 1;
        *slot = *built;
        slot->generation = generation;
        return id;
    }
    return gpu_vec_push(gpu_session.records, *built) ? gpu_session.records.count : 0;
}

static void edges_drop(ecs_entity_t dependent) {
    uint32_t kept = 0;
    for (uint32_t i = 0; i < gpu_session.edges.count; i++) {
        if (gpu_session.edges.items[i].dependent != dependent) {
            gpu_session.edges.items[kept++] = gpu_session.edges.items[i];
        }
    }
    gpu_session.edges.count = kept;
}

void gpu_edge_add(ecs_entity_t dependency, ecs_entity_t dependent) {
    for (uint32_t i = 0; i < gpu_session.edges.count; i++) {
        const gpu_edge_t edge = gpu_session.edges.items[i];
        if (edge.dependency == dependency && edge.dependent == dependent) {
            return;
        }
    }
    gpu_vec_push(gpu_session.edges, ((gpu_edge_t){ dependency, dependent }));
}

void gpu_wake(ecs_entity_t dependency) {
    for (uint32_t i = 0; i < gpu_session.edges.count; i++) {
        if (gpu_session.edges.items[i].dependency == dependency) {
            gpu_touch(gpu_session.edges.items[i].dependent, false);
        }
    }
}

/* Hands the SDL objects to GpuSync and clears the state to status. */
static void state_reset(GpuResourceState *state, uint8_t status) {
    if (state->record) {
        gpu_doom(state->record);
        state->record = 0;
    }
    state->status = status;
    state->error = GpuErrorNone;
    state->dependency = 0;
    state->message[0] = '\0';
}

void gpu_touch(ecs_entity_t entity, bool rebuild) {
    GpuResourceState *state = gpu_state(entity);
    if (!state) {
        return;
    }
    if (rebuild) {
        state_reset(state, GpuPending);
    }
    if (!state->queued) {
        state->queued = true;
        gpu_vec_push(gpu_session.queue, entity);
    }
}

void gpu_descriptor_changed(ecs_entity_t entity) {
    GpuResourceState *state = gpu_state(entity);
    if (state) {
        state->requested++;
        gpu_touch(entity, true);
    }
}

void gpu_descriptor_removed(ecs_entity_t entity) {
    GpuResourceState *state = gpu_state(entity);
    if (state) {
        state_reset(state, GpuRetiring);
    }
    edges_drop(entity);
    gpu_wake(entity);
}

void gpu_result_set(gpu_result_t *result, uint8_t status, uint8_t error,
                       ecs_entity_t dependency, const char *format, ...) {
    result->status = status;
    result->error = error;
    result->dependency = dependency;
    va_list args;
    va_start(args, format);
    vsnprintf(result->message, sizeof(result->message), format, args);
    va_end(args);
}

static void wait_for(gpu_result_t *result, ecs_entity_t dependency, const char *what) {
    gpu_result_set(result, GpuWaiting, GpuErrorNone, dependency, "waiting for %s", what);
}

static void ready(gpu_result_t *result, gpu_resource_kind_t kind) {
    result->status = GpuReady;
    result->built.kind = (uint8_t)kind;
}

/* Record of a dependency that must be Ready, or NULL with the result set. */
const gpu_record_t *gpu_dependency_ready(
    gpu_result_t *result,
    ecs_entity_t dependency,
    gpu_resource_kind_t kind,
    const char *what
) {
    if (!dependency || !ecs_is_alive(dependency) || gpu_resource_kind(dependency) != kind) {
        gpu_fail(result, GpuErrorDependency, dependency, "%s is not a live resource of that kind", what);
        return NULL;
    }
    const GpuResourceState *state = ecs_get(dependency, GpuResourceState);
    const gpu_record_t *record = gpu_record_at(state->record);
    if (state->status != GpuReady || !record) {
        wait_for(result, dependency, what);
        return NULL;
    }
    return record;
}

static void build_buffer(ecs_entity_t entity, gpu_result_t *result) {
    const GpuBuffer *desc = ecs_get(entity, GpuBuffer);
    if (!desc->size || desc->size % 4) {
        gpu_fail(result, GpuErrorInvalid, 0, "size must be a non-zero multiple of 4");
        return;
    }
    if (!desc->usage || desc->usage & ~GPU_BUFFER_USAGE_ALL) {
        gpu_fail(result, GpuErrorInvalid, 0, "usage must combine GpuBufferUsage flags");
        return;
    }
    if (desc->policy > GpuUploadStream) {
        gpu_fail(result, GpuErrorInvalid, 0, "policy is not a GpuUploadPolicy");
        return;
    }
    SDL_GPUBuffer *buffer = gpu_create_buffer(desc->size, desc->usage);
    if (!buffer) {
        gpu_fail(result, GpuErrorSdl, 0, "SDL refused the buffer: %s", SDL_GetError());
        return;
    }
    result->built.as.buffer.buffer = buffer;
    result->built.as.buffer.size = desc->size;
    result->built.as.buffer.policy = desc->policy;
    ready(result, GPU_RESOURCE_BUFFER);
}

/* True when following size_from from texture loops, or goes deeper than
 * any sane chain. */
static bool size_from_loops(ecs_entity_t texture) {
    ecs_entity_t at = texture;
    for (uint32_t i = 0; i < GPU_SIZE_FROM_DEPTH; i++) {
        const GpuTexture *desc = ecs_is_alive(at) ? ecs_try_get(at, GpuTexture) : NULL;
        if (!desc || !desc->size_from) {
            return false;
        }
        at = desc->size_from;
        if (at == texture) {
            return true;
        }
    }
    return true;
}

static uint32_t scaled(uint32_t size, float scale) {
    const float value = floorf((float)size * scale + 0.5f);
    return value < 1.0f ? 1 : value >= 4294967040.0f ? UINT32_MAX : (uint32_t)value;
}

static uint32_t mip_count(uint32_t size) {
    uint32_t count = 1;
    while (size > 1) {
        size >>= 1;
        count++;
    }
    return count;
}

/* current: the texture's objects when it is Ready and only a dependency
 * changed; kept as they are if the resolved size did not change. */
static void build_texture(ecs_entity_t entity, gpu_result_t *result, const gpu_record_t *current) {
    const GpuTexture *desc = ecs_get(entity, GpuTexture);
    const bool depth_format = gpu_format_depth(desc->format);
    const uint32_t samples = desc->samples ? desc->samples : 1;
    const uint32_t targets = GpuTextureColorTarget | GpuTextureDepthTarget;
    if (!gpu_format_size(desc->format)) {
        gpu_fail(result, GpuErrorInvalid, 0, "format is not a GpuFormat");
        return;
    }
    if (desc->dimension > GpuTextureCube) {
        gpu_fail(result, GpuErrorInvalid, 0, "dimension is not a GpuTextureDimension");
        return;
    }
    if (!desc->usage || desc->usage & ~GPU_TEXTURE_USAGE_ALL) {
        gpu_fail(result, GpuErrorInvalid, 0, "usage must combine GpuTextureUsage flags");
        return;
    }
    if (depth_format ? desc->usage & ~(GpuTextureSampled | GpuTextureDepthTarget)
                      : desc->usage & GpuTextureDepthTarget) {
        gpu_fail(result, GpuErrorInvalid, 0, "depth formats allow sampling and depth targets only");
        return;
    }
    if (samples != 1 && samples != 2 && samples != 4 && samples != 8) {
        gpu_fail(result, GpuErrorInvalid, 0, "samples must be 1, 2, 4 or 8");
        return;
    }
    if (samples > 1 && (desc->dimension != GpuTexture2d || desc->mips > 1 ||
                        desc->usage & ~targets)) {
        gpu_fail(result, GpuErrorInvalid, 0, "multisampled textures are 2D render targets");
        return;
    }
    if (!isfinite(desc->scale) || desc->scale < 0.0f) {
        gpu_fail(result, GpuErrorInvalid, 0, "scale must be positive");
        return;
    }
    uint32_t width = desc->width;
    uint32_t height = desc->height;
    if (desc->size_from) {
        gpu_edge_add(desc->size_from, entity);
        if (size_from_loops(entity)) {
            gpu_fail(result, GpuErrorCycle, desc->size_from, "size_from loops back to this texture");
            return;
        }
        const bool surface = ecs_is_alive(desc->size_from) &&
                             gpu_resource_kind(desc->size_from) == GPU_RESOURCE_SURFACE;
        const gpu_record_t *source = gpu_dependency_ready(
            result,
            desc->size_from,
            surface ? GPU_RESOURCE_SURFACE : GPU_RESOURCE_TEXTURE,
            "size_from"
        );
        if (!source) {
            return;
        }
        const float scale = desc->scale > 0.0f ? desc->scale : 1.0f;
        width = scaled(surface ? source->as.surface.width : source->as.texture.info.width, scale);
        height =
            scaled(surface ? source->as.surface.height : source->as.texture.info.height, scale);
    } else if (!width || !height) {
        gpu_fail(result, GpuErrorInvalid, 0, "set width and height, or size_from");
        return;
    }
    uint32_t depth = desc->depth ? desc->depth : 1;
    if (desc->dimension == GpuTexture2d) {
        depth = 1;
    } else if (desc->dimension == GpuTextureCube) {
        depth = 6;
        if (width != height) {
            gpu_fail(result, GpuErrorInvalid, 0, "cube faces must be square");
            return;
        }
    }
    const uint32_t largest = width > height ? width : height;
    const uint32_t mips = desc->mips ? desc->mips : 1;
    if (mips > mip_count(desc->dimension == GpuTexture3d && depth > largest ? depth : largest)) {
        gpu_fail(result, GpuErrorInvalid, 0, "more mips than the size allows");
        return;
    }
    const gpu_texture_info_t info = {
        .dimension = desc->dimension, .format = desc->format, .mips = (uint8_t)mips,
        .samples = (uint8_t)samples, .usage = desc->usage, .width = width, .height = height,
        .depth = depth,
    };
    if (current && current->kind == GPU_RESOURCE_TEXTURE &&
        memcmp(&current->as.texture.info, &info, sizeof(info)) == 0) {
        result->kept = true;
        ready(result, GPU_RESOURCE_TEXTURE);
        return;
    }
    SDL_GPUTexture *texture = gpu_create_texture(&info);
    if (!texture) {
        gpu_fail(result, GpuErrorSdl, 0, "SDL refused the texture: %s", SDL_GetError());
        return;
    }
    result->built.as.texture.texture = texture;
    result->built.as.texture.info = info;
    ready(result, GPU_RESOURCE_TEXTURE);
}

static void build_sampler(ecs_entity_t entity, gpu_result_t *result) {
    const GpuSampler *desc = ecs_get(entity, GpuSampler);
    if (desc->min_filter > GpuFilterLinear || desc->mag_filter > GpuFilterLinear ||
        desc->mip_filter > GpuFilterLinear) {
        gpu_fail(result, GpuErrorInvalid, 0, "filters must be GpuFilter values");
        return;
    }
    if (desc->address_u > GpuAddressClamp || desc->address_v > GpuAddressClamp ||
        desc->address_w > GpuAddressClamp) {
        gpu_fail(result, GpuErrorInvalid, 0, "address modes must be GpuAddress values");
        return;
    }
    if (desc->compare > GpuCompareAlways || desc->max_anisotropy > 16) {
        gpu_fail(result, GpuErrorInvalid, 0, "compare or max_anisotropy out of range");
        return;
    }
    SDL_GPUSampler *sampler = gpu_create_sampler(desc);
    if (!sampler) {
        gpu_fail(result, GpuErrorSdl, 0, "SDL refused the sampler: %s", SDL_GetError());
        return;
    }
    result->built.as.sampler = sampler;
    ready(result, GPU_RESOURCE_SAMPLER);
}

/* A registered asset, else a file read with SDL_LoadFile. *owned tells
 * whether data must be released with SDL_free. */
static bool pack_bytes(const char *path, const void **data, size_t *size, bool *owned) {
    const gpu_asset_t *asset = gpu_asset_find(path);
    if (asset) {
        *data = asset->data;
        *size = asset->size;
        *owned = false;
        return true;
    }
    *data = SDL_LoadFile(path, size);
    *owned = true;
    return *data != NULL;
}

static void build_shader(ecs_entity_t entity, gpu_result_t *result) {
    const GpuShader *desc = ecs_get(entity, GpuShader);
    if (!memchr(desc->path, 0, sizeof(desc->path)) || !desc->path[0]) {
        gpu_fail(result, GpuErrorInvalid, 0, "path is empty or not terminated");
        return;
    }
    const void *data = NULL;
    size_t size = 0;
    bool owned = false;
    if (!pack_bytes(desc->path, &data, &size, &owned)) {
        gpu_fail(result, GpuErrorPack, 0, "no asset or readable file named '%s'", desc->path);
        return;
    }
    const char *error = NULL;
    if (desc->fingerprint && gpu_pack_fingerprint(data, size) != desc->fingerprint) {
        error = "the fingerprint does not match";
    } else {
        error = gpu_pack_open(&result->built.as.pack, data, size);
    }
    if (owned) {
        SDL_free((void *)data);
    }
    if (error) {
        gpu_fail(result, GpuErrorPack, 0, "pack '%s': %s", desc->path, error);
        return;
    }
    ready(result, GPU_RESOURCE_SHADER);
}

static uint32_t entry_index(const gpu_pack_t *pack, const char *name, uint32_t stage) {
    const gpu_pack_entry_t *entry = gpu_pack_entry(pack, name);
    return entry && entry->stage == stage ? (uint32_t)(entry - pack->entries) : GPU_NONE;
}

static bool terminated(const char *name) { return memchr(name, 0, GPU_ENTRY_SIZE) != NULL; }

/* Creates the shaders of one graphics stage; vertex is required, an empty
 * fragment draws depth only (variants). */
static bool build_stage(gpu_result_t *result, const gpu_pack_t *pack, ecs_entity_t shader,
                        gpu_program_stage_t *stage, ecs_entity_t phase, const char *vertex,
                        const char *fragment, const GpuRaster *raster, bool fragment_required) {
    stage->used = true;
    stage->phase = phase;
    stage->raster = *raster;
    stage->vertex_entry = entry_index(pack, vertex, GpuPackVertex);
    stage->fragment_entry = fragment[0] ? entry_index(pack, fragment, GpuPackFragment) : GPU_NONE;
    if (stage->vertex_entry == GPU_NONE ||
        ((fragment[0] || fragment_required) && stage->fragment_entry == GPU_NONE)) {
        const bool vertex_missing = stage->vertex_entry == GPU_NONE;
        gpu_fail(result, GpuErrorEntry, shader, "no %s entry '%s'",
                 vertex_missing ? "vertex" : "fragment", vertex_missing ? vertex : fragment);
        return false;
    }
    if (raster->blend > GpuBlendPremultiplied || raster->topology > GpuTopologyPoints ||
        raster->cull > GpuFaceFront || raster->depth_compare > GpuCompareAlways) {
        gpu_fail(result, GpuErrorInvalid, 0, "raster state out of range");
        return false;
    }
    stage->vertex = gpu_create_shader(pack, &pack->entries[stage->vertex_entry]);
    stage->fragment = stage->fragment_entry == GPU_NONE
                          ? NULL
                          : gpu_create_shader(pack, &pack->entries[stage->fragment_entry]);
    if (!stage->vertex || (stage->fragment_entry != GPU_NONE && !stage->fragment)) {
        gpu_fail(result, GpuErrorSdl, 0, "SDL refused the program's shaders: %s", SDL_GetError());
        return false;
    }
    return true;
}

static void build_program(ecs_entity_t entity, gpu_result_t *result) {
    const GpuProgram *desc = ecs_get(entity, GpuProgram);
    bool names_ok = terminated(desc->vertex) && terminated(desc->fragment) &&
                    terminated(desc->compute);
    for (uint32_t i = 0; i < GPU_PROGRAM_VARIANTS; i++) {
        names_ok = names_ok && terminated(desc->variants[i].vertex) &&
                   terminated(desc->variants[i].fragment);
    }
    if (!names_ok) {
        gpu_fail(result, GpuErrorInvalid, 0, "entry names must be terminated");
        return;
    }
    const bool compute = desc->compute[0] != '\0';
    if (compute ? desc->vertex[0] || desc->fragment[0] : !desc->vertex[0] || !desc->fragment[0]) {
        gpu_fail(result, GpuErrorInvalid, 0, "set vertex and fragment, or compute alone");
        return;
    }
    for (uint32_t i = 0; i < GPU_PROGRAM_VARIANTS; i++) {
        const GpuProgramVariant *variant = &desc->variants[i];
        if (variant->phase && (compute || !variant->vertex[0])) {
            gpu_fail(result, GpuErrorInvalid, 0, "variant %u needs a vertex entry", i);
            return;
        }
    }
    if (desc->shader) {
        gpu_edge_add(desc->shader, entity);
    }
    const gpu_record_t *shader =
        gpu_dependency_ready(result, desc->shader, GPU_RESOURCE_SHADER, "shader");
    if (!shader) {
        return;
    }
    const gpu_pack_t *pack = &shader->as.pack;
    gpu_record_t *built = &result->built;
    built->kind = GPU_RESOURCE_PROGRAM;
    built->as.program.shader = desc->shader;
    built->as.program.compute_entry = GPU_NONE;
    for (uint32_t i = 0; i < GPU_PROGRAM_STAGES; i++) {
        built->as.program.stages[i].vertex_entry = GPU_NONE;
        built->as.program.stages[i].fragment_entry = GPU_NONE;
    }
    if (compute) {
        const uint32_t index = entry_index(pack, desc->compute, GpuPackCompute);
        if (index == GPU_NONE) {
            gpu_fail(result, GpuErrorEntry, desc->shader, "no compute entry '%s'", desc->compute);
            built->kind = GPU_RESOURCE_NONE;
            return;
        }
        built->as.program.compute_entry = index;
        built->as.program.compute = gpu_create_compute_pipeline(pack, &pack->entries[index]);
        if (!built->as.program.compute) {
            gpu_fail(result, GpuErrorSdl, 0, "SDL refused compute entry '%s': %s", desc->compute,
                     SDL_GetError());
            built->kind = GPU_RESOURCE_NONE;
            return;
        }
        ready(result, GPU_RESOURCE_PROGRAM);
        return;
    }
    bool ok = build_stage(result, pack, desc->shader, &built->as.program.stages[0], desc->phase,
                          desc->vertex, desc->fragment, &desc->raster, true);
    for (uint32_t i = 0; ok && i < GPU_PROGRAM_VARIANTS; i++) {
        const GpuProgramVariant *variant = &desc->variants[i];
        if (variant->phase) {
            ok = build_stage(result, pack, desc->shader, &built->as.program.stages[i + 1],
                             variant->phase, variant->vertex, variant->fragment, &variant->raster,
                             false);
        }
    }
    if (!ok) {
        gpu_record_release(built);
        return;
    }
    ready(result, GPU_RESOURCE_PROGRAM);
}

/* Writes the session state; it only changes when a device opens, closes
 * or is lost. */
static void session_publish(void) {
    SDL_GPUDevice *gpu = gpu_session.gpu;
    GpuSession *session = ecs_get_resource(GpuSession);
    *session = (GpuSession){
        .device = gpu_session.device,
        .ready = gpu && !gpu_session.lost,
        .lost = gpu && gpu_session.lost,
        .frames_in_flight = gpu ? gpu_session.frames_in_flight : 0,
        .submissions = gpu_session.submissions,
    };
    if (gpu) {
        SDL_strlcpy(session->driver, SDL_GetGPUDeviceDriver(gpu), sizeof(session->driver));
    }
}

/* Installs the result on the entity. Its dependents are woken when the
 * status or the SDL objects changed. A Ready result without objects (the
 * device) still counts as a new allocation. */
static void apply(ecs_entity_t entity, gpu_result_t *result) {
    GpuResourceState *state = ecs_get(entity, GpuResourceState);
    const uint8_t previous = state->status;
    bool replaced = result->status == GpuReady && !result->kept;
    if (replaced && result->built.kind) {
        result->built.entity = entity;
        const uint32_t id = record_store(&result->built);
        if (id) {
            record_release(state->record);
            state->record = id;
        } else {
            gpu_record_release(&result->built);
            gpu_fail(result, GpuErrorSdl, 0, "out of memory");
            replaced = false;
        }
    }
    if (result->status != GpuReady && state->record) {
        record_release(state->record);
        state->record = 0;
    }
    const bool changed = replaced || result->resized || previous != result->status ||
                         strcmp(state->message, result->message) != 0;
    state->status = result->status;
    state->error = result->error;
    state->dependency = result->dependency;
    memcpy(state->message, result->message, sizeof(state->message));
    if (result->status == GpuReady) {
        state->applied = state->requested;
        state->allocation += replaced;
    }
    if (result->status == GpuFailed && changed) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "sigpu: %s failed: %s", ecs_entity_name(entity),
                     result->message);
    }
    if (changed) {
        ecs_modified_cid(entity, ecs_id(GpuResourceState));
        gpu_wake(entity);
    }
}

static bool process_resource(ecs_entity_t entity) {
    GpuResourceState *state = gpu_state(entity);
    if (!state || !state->queued) {
        return false;
    }
    state->queued = false;
    const gpu_resource_kind_t kind = gpu_resource_kind(entity);
    /* Devices are handled by sync_devices. */
    if (kind == GPU_RESOURCE_NONE || kind == GPU_RESOURCE_DEVICE) {
        return false;
    }
    gpu_stats()->processed++;
    edges_drop(entity);
    gpu_result_t result = { 0 };
    switch (kind) {
    case GPU_RESOURCE_BUFFER:
        build_buffer(entity, &result);
        break;
    case GPU_RESOURCE_TEXTURE:
        build_texture(entity, &result, gpu_record_at(state->record));
        break;
    case GPU_RESOURCE_SAMPLER:
        build_sampler(entity, &result);
        break;
    case GPU_RESOURCE_SHADER:
        build_shader(entity, &result);
        break;
    case GPU_RESOURCE_PROGRAM:
        build_program(entity, &result);
        break;
    case GPU_RESOURCE_GEOMETRY:
        gpu_geometry_build(entity, &result);
        break;
    case GPU_RESOURCE_SURFACE:
        gpu_surface_build(entity, &result, gpu_record_at(state->record));
        break;
    case GPU_RESOURCE_MATERIAL:
        gpu_material_build(entity, &result);
        break;
    default:
        gpu_fail(&result, GpuErrorInvalid, 0, "an entity holds a single GPU descriptor");
        break;
    }
    apply(entity, &result);
    return true;
}

/* Releases every record now: the device is about to close. */
static void release_all(bool final) {
    release_doomed();
    for (uint32_t id = 1; id <= gpu_session.records.count; id++) {
        if (!gpu_record_at(id)) {
            continue;
        }
        const ecs_entity_t owner = gpu_session.records.items[id - 1].entity;
        record_release(id);
        GpuResourceState *state = final ? NULL : gpu_state(owner);
        if (state && state->record == id) {
            state->record = 0;
        }
    }
}

/* Unless final, every resource goes back to Pending for the next device,
 * IsA bases (Abstract) and disabled entities included. */
static void device_close(bool final) {
    if (!gpu_session.gpu) {
        return;
    }
    release_all(final);
    gpu_pipelines_release();
    gpu_features_release();
    gpu_materials_release();
    gpu_batches_release();
    gpu_staging_release();
    gpu_close_device();
    gpu_session.device = 0;
    gpu_session.submissions = 0;
    gpu_session.lost = false;
    gpu_session.lost_reported = false;
    if (final) {
        return;
    }
    session_publish();
    GPU_VEC(ecs_entity_t) owners = { 0 };
    ecs_query_each(it, i, ecs_filter(GpuResourceState), ecs_not(GpuDevice),
                   ecs_in_optional(Abstract), ecs_in_optional(Disabled)) {
        gpu_vec_push(owners, it.entities[i]);
    }
    for (uint32_t i = 0; i < owners.count; i++) {
        /* A Retiring state without descriptor stays as it is. */
        if (gpu_resource_kind(owners.items[i]) != GPU_RESOURCE_NONE) {
            gpu_touch(owners.items[i], true);
            ecs_modified_cid(owners.items[i], ecs_id(GpuResourceState));
        }
    }
    gpu_vec_free(owners);
}

static void build_device(ecs_entity_t entity, gpu_result_t *result) {
    if (gpu_resource_kind(entity) == GPU_RESOURCE_MANY) {
        gpu_fail(result, GpuErrorInvalid, 0, "an entity holds a single GPU descriptor");
        return;
    }
    if (gpu_session.device) {
        gpu_result_set(result, GpuWaiting, GpuErrorNone, gpu_session.device,
                   "waiting for the open device to close");
        return;
    }
    const GpuDevice *desc = ecs_get(entity, GpuDevice);
    if (desc->frames_in_flight > GPU_MAX_FRAMES_IN_FLIGHT) {
        gpu_fail(result, GpuErrorInvalid, 0, "frames_in_flight is at most 3");
        return;
    }
    if (!gpu_open_device(desc->debug)) {
        gpu_fail(result, GpuErrorDevice, 0, "SDL GPU device creation failed: %s", SDL_GetError());
        return;
    }
    gpu_session.device = entity;
    gpu_session.frames_in_flight = desc->frames_in_flight ? desc->frames_in_flight : 2;
    session_publish();
    result->status = GpuReady;
}

static void open_device(ecs_entity_t entity) {
    gpu_result_t result = { 0 };
    build_device(entity, &result);
    apply(entity, &result);
}

/* One device per process. The open one closes when its descriptor went
 * away, and reopens first when it changed; then every other device opens
 * if none is open, or waits. */
static void sync_devices(void) {
    if (!gpu_session.device_dirty) {
        return;
    }
    gpu_session.device_dirty = false;
    const ecs_entity_t device = gpu_session.device;
    const GpuResourceState *open = gpu_state(device);
    const bool kept = open && ecs_has(device, GpuDevice);
    if (device && (!kept || open->status == GpuPending)) {
        device_close(false);
        if (kept) {
            open_device(device);
        }
    }
    GPU_VEC(ecs_entity_t) devices = { 0 };
    ecs_query_each(it, i, ecs_filter(GpuDevice), ecs_filter(GpuResourceState)) {
        gpu_vec_push(devices, it.entities[i]);
    }
    for (uint32_t i = 0; i < devices.count; i++) {
        if (devices.items[i] != gpu_session.device) {
            open_device(devices.items[i]);
        }
    }
    gpu_vec_free(devices);
}

void gpu_mark_lost(const char *what) {
    if (!gpu_session.lost) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "sigpu: %s failed, device treated as lost: %s", what,
                     SDL_GetError());
        gpu_session.lost = true;
    }
}

static void report_lost(void) {
    if (!gpu_session.gpu || !gpu_session.lost || gpu_session.lost_reported) {
        return;
    }
    gpu_session.lost_reported = true;
    session_publish();
    const ecs_entity_t device = gpu_session.device;
    gpu_result_t result = { 0 };
    gpu_fail(&result, GpuErrorDevice, 0, "device lost");
    apply(device, &result);
    ecs_observer_trigger(device, gpu_session.events[GpuDeviceLost], &device);
}

void gpu_sync(ecs_iter_t *it) {
    (void)it;
    const uint64_t start = SDL_GetTicksNS();
    GpuStats *counters = gpu_stats();
    *counters = (GpuStats){ .objects = counters->objects,
                            .memory_reserved = counters->memory_reserved };
    gpu_session.frame++;
    report_lost();
    release_doomed();
    sync_devices();
    if (gpu_session.gpu && !gpu_session.lost) {
        uint32_t processed = 0;
        const uint32_t budget = gpu_session.prewarm ? UINT32_MAX : gpu_session.budget;
        while (gpu_session.queue_head < gpu_session.queue.count && processed < budget) {
            processed += process_resource(gpu_session.queue.items[gpu_session.queue_head++]);
        }
        if (gpu_session.queue_head == gpu_session.queue.count) {
            gpu_session.queue.count = 0;
            gpu_session.queue_head = 0;
        } else if (gpu_session.queue_head > gpu_session.queue.count / 2) {
            memmove(gpu_session.queue.items, gpu_session.queue.items + gpu_session.queue_head,
                    (gpu_session.queue.count - gpu_session.queue_head) * sizeof(ecs_entity_t));
            gpu_session.queue.count -= gpu_session.queue_head;
            gpu_session.queue_head = 0;
        }
    }
    if (gpu_session.gpu && !gpu_session.lost) {
        gpu_surfaces_sync();
    }
    gpu_stats()->deferred = gpu_session.queue.count - gpu_session.queue_head;
    gpu_stats()->sync_ns = SDL_GetTicksNS() - start;
}

void gpu_session_shutdown(void *data) {
    (void)data;
    device_close(true);
    gpu_graphs_fini();
    gpu_features_fini();
    gpu_materials_fini();
    gpu_batches_fini();
    gpu_surfaces_fini();
    for (uint32_t i = 0; i < gpu_session.uploads.count; i++) {
        if (gpu_session.uploads.items[i].owned) {
            free((void *)gpu_session.uploads.items[i].data);
        }
    }
    for (uint32_t i = 0; i < gpu_session.assets.count; i++) {
        free(gpu_session.assets.items[i].data);
    }
    gpu_vec_free(gpu_session.queue);
    gpu_vec_free(gpu_session.doomed);
    gpu_vec_free(gpu_session.edges);
    gpu_vec_free(gpu_session.records);
    gpu_vec_free(gpu_session.free_records);
    gpu_vec_free(gpu_session.assets);
    gpu_vec_free(gpu_session.uploads);
    gpu_vec_free(gpu_session.pipelines);
    gpu_session = (gpu_session_t){ 0 };
}

SDL_GPUDevice *gpu_session_device(void) { return gpu_session.gpu; }

void sigpu_prewarm(void) { gpu_session.prewarm = gpu_session.open; }

bool sigpu_extent(ecs_entity_t resource, uint32_t *width, uint32_t *height) {
    const gpu_record_t *record = gpu_resource_record(resource, GPU_RESOURCE_TEXTURE);
    const gpu_record_t *surface = record ? NULL : gpu_resource_record(resource, GPU_RESOURCE_SURFACE);
    if (!record && !surface) {
        return false;
    }
    if (width) {
        *width = record ? record->as.texture.info.width : surface->as.surface.width;
    }
    if (height) {
        *height = record ? record->as.texture.info.height : surface->as.surface.height;
    }
    return true;
}

const gpu_record_t *gpu_resource_record(ecs_entity_t entity, uint8_t kind) {
    const GpuResourceState *state = gpu_state(entity);
    const gpu_record_t *record = state && state->status == GpuReady ? gpu_record_at(state->record) : NULL;
    return record && record->kind == kind ? record : NULL;
}

SDL_GPUBuffer *gpu_resource_buffer(ecs_entity_t buffer) {
    const gpu_record_t *record = gpu_resource_record(buffer, GPU_RESOURCE_BUFFER);
    return record ? record->as.buffer.buffer : NULL;
}

SDL_GPUTexture *gpu_resource_texture(ecs_entity_t texture, gpu_texture_info_t *info) {
    const gpu_record_t *record = gpu_resource_record(texture, GPU_RESOURCE_TEXTURE);
    if (info) {
        *info = record ? record->as.texture.info : (gpu_texture_info_t){ 0 };
    }
    return record ? record->as.texture.texture : NULL;
}

const gpu_pack_entry_t *gpu_pack_entry_at(const gpu_pack_t *pack, uint32_t index) {
    return pack && index < pack->header->entry_count ? &pack->entries[index] : NULL;
}

const gpu_pack_t *gpu_program_pack(const gpu_record_t *program) {
    const gpu_record_t *shader =
        program ? gpu_resource_record(program->as.program.shader, GPU_RESOURCE_SHADER) : NULL;
    return shader ? &shader->as.pack : NULL;
}

const gpu_pack_t *gpu_resource_program_pack(
    ecs_entity_t program,
    const gpu_pack_entry_t **vertex,
    const gpu_pack_entry_t **fragment,
    const gpu_pack_entry_t **compute
) {
    const gpu_record_t *record = gpu_resource_record(program, GPU_RESOURCE_PROGRAM);
    const gpu_pack_t *pack = gpu_program_pack(record);
    if (vertex) {
        *vertex = pack ? gpu_pack_entry_at(pack, record->as.program.stages[0].vertex_entry) : NULL;
    }
    if (fragment) {
        *fragment =
            pack ? gpu_pack_entry_at(pack, record->as.program.stages[0].fragment_entry) : NULL;
    }
    if (compute) {
        *compute = pack ? gpu_pack_entry_at(pack, record->as.program.compute_entry) : NULL;
    }
    return pack;
}
