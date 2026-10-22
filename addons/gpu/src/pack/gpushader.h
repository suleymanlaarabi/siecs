#ifndef SIGPU_GPUSHADER_H
#define SIGPU_GPUSHADER_H

#include "pack/gpushader_format.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A validated .gpushader pack. It owns a copy of the file, so the caller's
 * buffer can go away after gpu_pack_open. Parsing needs no SDL. */
typedef struct {
    void *data;
    const gpu_pack_header_t *header;
    const gpu_pack_entry_t *entries;
    const gpu_pack_resource_t *resources;
    const gpu_pack_field_t *fields;
    const gpu_pack_input_t *inputs;
} gpu_pack_t;

/* NULL on success, else why the pack is malformed or incompatible; the pack
 * is then empty. */
const char *gpu_pack_open(gpu_pack_t *pack, const void *data, size_t size);
void gpu_pack_close(gpu_pack_t *pack);

const gpu_pack_entry_t *gpu_pack_entry(const gpu_pack_t *pack, const char *name);
const gpu_pack_resource_t *gpu_pack_resource(
    const gpu_pack_t *pack,
    const gpu_pack_entry_t *entry,
    const char *name
);
const gpu_pack_field_t *gpu_pack_field(
    const gpu_pack_t *pack,
    const gpu_pack_resource_t *resource,
    const char *name
);
const gpu_pack_input_t *gpu_pack_input(
    const gpu_pack_t *pack,
    const gpu_pack_entry_t *entry,
    const char *name
);
const uint8_t *gpu_pack_code(const gpu_pack_t *pack, const gpu_pack_entry_t *entry);

/* FNV-1a 64-bit, the hash GpuShader.fingerprint compares against. */
uint64_t gpu_pack_fingerprint(const void *data, size_t size);

#endif
