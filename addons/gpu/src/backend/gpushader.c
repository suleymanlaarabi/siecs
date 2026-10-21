#include "backend/gpushader.h"
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(gpu_pack_header_t) % 4 == 0, "pack header must stay 4-aligned");
_Static_assert(sizeof(gpu_pack_entry_t) % 4 == 0, "pack entry must stay 4-aligned");

static bool pack_error(const char *message) {
    SDL_LogError(SDL_LOG_CATEGORY_GPU, "sigpu: invalid shader pack: %s", message);
    return false;
}

static bool name_valid(const char *name) {
    return memchr(name, 0, GPU_PACK_NAME_SIZE) != NULL;
}

static bool range_valid(uint32_t first, uint32_t count, uint32_t total) {
    return first <= total && count <= total - first;
}

static bool pack_validate(const gpu_pack_t *pack, size_t size) {
    const gpu_pack_header_t *header = pack->header;
    for (uint32_t i = 0; i < header->entry_count; i++) {
        const gpu_pack_entry_t *entry = &pack->entries[i];
        if (!name_valid(entry->name) || entry->stage > GpuPackCompute) {
            return pack_error("bad entry");
        }
        if (entry->target != GpuPackSpirv) {
            return pack_error("unsupported target");
        }
        if (entry->code_offset % 4 || entry->code_size == 0 || entry->code_offset > size ||
            entry->code_size > size - entry->code_offset) {
            return pack_error("entry code out of bounds");
        }
        if (!range_valid(entry->first_resource, entry->resource_count, header->resource_count) ||
            !range_valid(entry->first_input, entry->input_count, header->input_count)) {
            return pack_error("entry tables out of bounds");
        }
    }
    for (uint32_t i = 0; i < header->resource_count; i++) {
        const gpu_pack_resource_t *resource = &pack->resources[i];
        if (!name_valid(resource->name) || resource->kind >= GpuPackKindCount ||
            !range_valid(resource->first_field, resource->field_count, header->field_count)) {
            return pack_error("bad resource");
        }
    }
    for (uint32_t i = 0; i < header->field_count; i++) {
        if (!name_valid(pack->fields[i].name)) {
            return pack_error("bad field");
        }
    }
    for (uint32_t i = 0; i < header->input_count; i++) {
        if (!name_valid(pack->inputs[i].name)) {
            return pack_error("bad input");
        }
    }
    return true;
}

bool gpu_pack_open(gpu_pack_t *pack, const void *data, size_t size) {
    *pack = (gpu_pack_t){ 0 };
    gpu_pack_header_t header;
    if (size < sizeof(header)) {
        return pack_error("truncated header");
    }
    memcpy(&header, data, sizeof(header));
    if (memcmp(header.magic, GPU_PACK_MAGIC, sizeof(header.magic)) != 0) {
        return pack_error("bad magic");
    }
    if (header.version != GPU_PACK_VERSION) {
        return pack_error("unsupported version, rebuild it with tools/gpushader");
    }
    if (header.file_size != size) {
        return pack_error("size mismatch");
    }
    const uint64_t tables = sizeof(header) + (uint64_t)header.entry_count * sizeof(gpu_pack_entry_t) +
                            (uint64_t)header.resource_count * sizeof(gpu_pack_resource_t) +
                            (uint64_t)header.field_count * sizeof(gpu_pack_field_t) +
                            (uint64_t)header.input_count * sizeof(gpu_pack_input_t);
    if (tables > size) {
        return pack_error("truncated tables");
    }
    /* malloc alignment covers every table; the caller's buffer may not. */
    uint8_t *copy = malloc(size);
    if (!copy) {
        return pack_error("out of memory");
    }
    memcpy(copy, data, size);
    pack->data = copy;
    pack->header = (const gpu_pack_header_t *)copy;
    pack->entries = (const gpu_pack_entry_t *)(copy + sizeof(header));
    pack->resources = (const gpu_pack_resource_t *)(pack->entries + header.entry_count);
    pack->fields = (const gpu_pack_field_t *)(pack->resources + header.resource_count);
    pack->inputs = (const gpu_pack_input_t *)(pack->fields + header.field_count);
    if (!pack_validate(pack, size)) {
        gpu_pack_close(pack);
        return false;
    }
    return true;
}

void gpu_pack_close(gpu_pack_t *pack) {
    free(pack->data);
    *pack = (gpu_pack_t){ 0 };
}

const gpu_pack_entry_t *gpu_pack_entry(const gpu_pack_t *pack, const char *name) {
    for (uint32_t i = 0; i < pack->header->entry_count; i++) {
        if (strcmp(pack->entries[i].name, name) == 0) {
            return &pack->entries[i];
        }
    }
    return NULL;
}

const gpu_pack_resource_t *gpu_pack_resource(
    const gpu_pack_t *pack,
    const gpu_pack_entry_t *entry,
    const char *name
) {
    for (uint32_t i = 0; i < entry->resource_count; i++) {
        const gpu_pack_resource_t *resource = &pack->resources[entry->first_resource + i];
        if (strcmp(resource->name, name) == 0) {
            return resource;
        }
    }
    return NULL;
}

const gpu_pack_field_t *gpu_pack_field(
    const gpu_pack_t *pack,
    const gpu_pack_resource_t *resource,
    const char *name
) {
    for (uint32_t i = 0; i < resource->field_count; i++) {
        const gpu_pack_field_t *field = &pack->fields[resource->first_field + i];
        if (strcmp(field->name, name) == 0) {
            return field;
        }
    }
    return NULL;
}

static const uint8_t *entry_code(const gpu_pack_t *pack, const gpu_pack_entry_t *entry) {
    return (const uint8_t *)pack->data + entry->code_offset;
}

SDL_GPUShader *gpu_pack_create_shader(
    SDL_GPUDevice *device,
    const gpu_pack_t *pack,
    const gpu_pack_entry_t *entry
) {
    if (entry->stage == GpuPackCompute) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "sigpu: '%s' is a compute entry", entry->name);
        return NULL;
    }
    SDL_GPUShaderCreateInfo info = {
        .code_size = entry->code_size,
        .code = entry_code(pack, entry),
        .entrypoint = entry->name,
        .format = SDL_GPU_SHADERFORMAT_SPIRV,
        .stage = entry->stage == GpuPackVertex ? SDL_GPU_SHADERSTAGE_VERTEX
                                               : SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = entry->counts[GpuPackSampler],
        .num_storage_textures = entry->counts[GpuPackStorageTexture],
        .num_storage_buffers = entry->counts[GpuPackStorageBuffer],
        .num_uniform_buffers = entry->counts[GpuPackUniformBuffer],
    };
    SDL_GPUShader *shader = SDL_CreateGPUShader(device, &info);
    if (!shader) {
        SDL_LogError(
            SDL_LOG_CATEGORY_GPU,
            "sigpu: unable to create shader '%s': %s",
            entry->name,
            SDL_GetError()
        );
    }
    return shader;
}

SDL_GPUComputePipeline *gpu_pack_create_compute_pipeline(
    SDL_GPUDevice *device,
    const gpu_pack_t *pack,
    const gpu_pack_entry_t *entry
) {
    if (entry->stage != GpuPackCompute) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "sigpu: '%s' is not a compute entry", entry->name);
        return NULL;
    }
    SDL_GPUComputePipelineCreateInfo info = {
        .code_size = entry->code_size,
        .code = entry_code(pack, entry),
        .entrypoint = entry->name,
        .format = SDL_GPU_SHADERFORMAT_SPIRV,
        .num_samplers = entry->counts[GpuPackSampler],
        .num_readonly_storage_textures = entry->counts[GpuPackStorageTexture],
        .num_readonly_storage_buffers = entry->counts[GpuPackStorageBuffer],
        .num_readwrite_storage_textures = entry->counts[GpuPackRWStorageTexture],
        .num_readwrite_storage_buffers = entry->counts[GpuPackRWStorageBuffer],
        .num_uniform_buffers = entry->counts[GpuPackUniformBuffer],
        .threadcount_x = entry->threads[0],
        .threadcount_y = entry->threads[1],
        .threadcount_z = entry->threads[2],
    };
    SDL_GPUComputePipeline *pipeline = SDL_CreateGPUComputePipeline(device, &info);
    if (!pipeline) {
        SDL_LogError(
            SDL_LOG_CATEGORY_GPU,
            "sigpu: unable to create compute pipeline '%s': %s",
            entry->name,
            SDL_GetError()
        );
    }
    return pipeline;
}
