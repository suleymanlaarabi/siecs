#include "pack/gpushader.h"
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(gpu_pack_header_t) % 4 == 0, "pack header must stay 4-aligned");
_Static_assert(sizeof(gpu_pack_entry_t) % 4 == 0, "pack entry must stay 4-aligned");

static bool name_valid(const char *name) {
    return memchr(name, 0, GPU_PACK_NAME_SIZE) != NULL;
}

static bool range_valid(uint32_t first, uint32_t count, uint32_t total) {
    return first <= total && count <= total - first;
}

static const char *pack_validate(const gpu_pack_t *pack, size_t size) {
    const gpu_pack_header_t *header = pack->header;
    if (!memchr(header->toolchain, 0, sizeof(header->toolchain))) {
        return "bad toolchain";
    }
    for (uint32_t i = 0; i < header->entry_count; i++) {
        const gpu_pack_entry_t *entry = &pack->entries[i];
        if (!name_valid(entry->name) || entry->stage > GpuPackCompute) {
            return "bad entry";
        }
        if (entry->target != GpuPackSpirv) {
            return "unsupported target";
        }
        if (entry->code_offset % 4 || entry->code_size == 0 || entry->code_offset > size ||
            entry->code_size > size - entry->code_offset) {
            return "entry code out of bounds";
        }
        if (!range_valid(entry->first_resource, entry->resource_count, header->resource_count) ||
            !range_valid(entry->first_input, entry->input_count, header->input_count)) {
            return "entry tables out of bounds";
        }
    }
    for (uint32_t i = 0; i < header->resource_count; i++) {
        const gpu_pack_resource_t *resource = &pack->resources[i];
        if (!name_valid(resource->name) || resource->kind >= GpuPackKindCount ||
            !range_valid(resource->first_field, resource->field_count, header->field_count)) {
            return "bad resource";
        }
    }
    for (uint32_t i = 0; i < header->field_count; i++) {
        if (!name_valid(pack->fields[i].name)) {
            return "bad field";
        }
    }
    for (uint32_t i = 0; i < header->input_count; i++) {
        if (!name_valid(pack->inputs[i].name)) {
            return "bad input";
        }
    }
    return NULL;
}

const char *gpu_pack_open(gpu_pack_t *pack, const void *data, size_t size) {
    *pack = (gpu_pack_t){ 0 };
    gpu_pack_header_t header;
    if (size < sizeof(header)) {
        return "truncated header";
    }
    memcpy(&header, data, sizeof(header));
    if (memcmp(header.magic, GPU_PACK_MAGIC, sizeof(header.magic)) != 0) {
        return "bad magic";
    }
    if (header.version != GPU_PACK_VERSION) {
        return "unsupported version, rebuild it with tools/gpushader";
    }
    if (header.file_size != size) {
        return "size mismatch";
    }
    const uint64_t tables = sizeof(header) + (uint64_t)header.entry_count * sizeof(gpu_pack_entry_t) +
                            (uint64_t)header.resource_count * sizeof(gpu_pack_resource_t) +
                            (uint64_t)header.field_count * sizeof(gpu_pack_field_t) +
                            (uint64_t)header.input_count * sizeof(gpu_pack_input_t);
    if (tables > size) {
        return "truncated tables";
    }
    /* malloc alignment covers every table; the caller's buffer may not. */
    uint8_t *copy = malloc(size);
    if (!copy) {
        return "out of memory";
    }
    memcpy(copy, data, size);
    pack->data = copy;
    pack->header = (const gpu_pack_header_t *)copy;
    pack->entries = (const gpu_pack_entry_t *)(copy + sizeof(header));
    pack->resources = (const gpu_pack_resource_t *)(pack->entries + header.entry_count);
    pack->fields = (const gpu_pack_field_t *)(pack->resources + header.resource_count);
    pack->inputs = (const gpu_pack_input_t *)(pack->fields + header.field_count);
    const char *error = pack_validate(pack, size);
    if (error) {
        gpu_pack_close(pack);
    }
    return error;
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

const gpu_pack_input_t *gpu_pack_input(
    const gpu_pack_t *pack,
    const gpu_pack_entry_t *entry,
    const char *name
) {
    for (uint32_t i = 0; i < entry->input_count; i++) {
        const gpu_pack_input_t *input = &pack->inputs[entry->first_input + i];
        if (strcmp(input->name, name) == 0) {
            return input;
        }
    }
    return NULL;
}

const uint8_t *gpu_pack_code(const gpu_pack_t *pack, const gpu_pack_entry_t *entry) {
    return (const uint8_t *)pack->data + entry->code_offset;
}

uint64_t gpu_pack_fingerprint(const void *data, size_t size) {
    const uint8_t *bytes = data;
    uint64_t hash = 14695981039346656037ull;
    for (size_t i = 0; i < size; i++) {
        hash = (hash ^ bytes[i]) * 1099511628211ull;
    }
    return hash;
}
