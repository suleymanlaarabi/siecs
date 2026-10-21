#ifndef SIGPU_GPUSHADER_H
#define SIGPU_GPUSHADER_H

#include "backend/gpushader_format.h"
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>

/* A validated .gpushader pack. It owns a copy of the file, so the caller's
 * buffer can go away after gpu_pack_open. */
typedef struct {
    void *data;
    const gpu_pack_header_t *header;
    const gpu_pack_entry_t *entries;
    const gpu_pack_resource_t *resources;
    const gpu_pack_field_t *fields;
    const gpu_pack_input_t *inputs;
} gpu_pack_t;

/* Logs the reason and returns false on a malformed or incompatible pack. */
bool gpu_pack_open(gpu_pack_t *pack, const void *data, size_t size);
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

/* NULL with a log on failure, or when the stage does not match. */
SDL_GPUShader *gpu_pack_create_shader(
    SDL_GPUDevice *device,
    const gpu_pack_t *pack,
    const gpu_pack_entry_t *entry
);
SDL_GPUComputePipeline *gpu_pack_create_compute_pipeline(
    SDL_GPUDevice *device,
    const gpu_pack_t *pack,
    const gpu_pack_entry_t *entry
);

#endif
