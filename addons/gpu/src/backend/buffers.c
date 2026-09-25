#include "backend/backend.h"

#include "backend/passes/passes.h"
#include <string.h>

static void resize_instances(
    SDL_GPUBuffer **buffer,
    SDL_GPUTransferBuffer **transfer,
    Uint32 capacity,
    Uint32 stride
) {
    if (*buffer) {
        SDL_ReleaseGPUBuffer(SIGPU_GPUCONTEXT->device, *buffer);
        SDL_ReleaseGPUTransferBuffer(SIGPU_GPUCONTEXT->device, *transfer);
    }

    Uint32 size = stride * capacity;
    *buffer = SDL_CreateGPUBuffer(
        SIGPU_GPUCONTEXT->device,
        &(SDL_GPUBufferCreateInfo){
            .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
            .size = size,
        }
    );
    *transfer = SDL_CreateGPUTransferBuffer(
        SIGPU_GPUCONTEXT->device,
        &(SDL_GPUTransferBufferCreateInfo){
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = size,
        }
    );
}

static void grow_instances(
    SDL_GPUBuffer **buffer,
    SDL_GPUTransferBuffer **transfer,
    void **mapped,
    Uint32 *capacity,
    Uint32 shared_stride,
    Uint32 shared_count,
    Uint32 owned_stride,
    Uint32 owned_count
) {
    const Uint32 old_capacity = *capacity;
    const Uint32 new_capacity = old_capacity * 2;
    SDL_GPUBuffer *old_buffer = *buffer;
    SDL_GPUTransferBuffer *old_transfer = *transfer;
    void *old_mapped = *mapped;

    *buffer = NULL;
    *transfer = NULL;
    resize_instances(buffer, transfer, new_capacity, owned_stride);
    *mapped = SDL_MapGPUTransferBuffer(SIGPU_GPUCONTEXT->device, *transfer, true);

    memcpy(*mapped, old_mapped, shared_count * shared_stride);
    memcpy(
        (uint8_t *)*mapped + (new_capacity - owned_count) * owned_stride,
        (uint8_t *)old_mapped + (old_capacity - owned_count) * owned_stride,
        owned_count * owned_stride
    );

    SDL_UnmapGPUTransferBuffer(SIGPU_GPUCONTEXT->device, old_transfer);
    SDL_ReleaseGPUBuffer(SIGPU_GPUCONTEXT->device, old_buffer);
    SDL_ReleaseGPUTransferBuffer(SIGPU_GPUCONTEXT->device, old_transfer);
    *capacity = new_capacity;
}

static void resize_axis_instances(Uint32 capacity) {
    resize_instances(
        &SIGPU_GPUCONTEXT->axis_buffer,
        &SIGPU_GPUCONTEXT->axis_transfer,
        capacity,
        sizeof(sigpu_axis_instance_t)
    );
    SIGPU_GPUCONTEXT->axis_capacity = capacity;
}

static void resize_rotated_instances(Uint32 capacity) {
    resize_instances(
        &SIGPU_GPUCONTEXT->rotated_buffer,
        &SIGPU_GPUCONTEXT->rotated_transfer,
        capacity,
        sizeof(sigpu_rotated_instance_t)
    );
    SIGPU_GPUCONTEXT->rotated_capacity = capacity;
}

static SDL_GPUBuffer *create_static_buffer(Uint32 size) {
    return SDL_CreateGPUBuffer(
        SIGPU_GPUCONTEXT->device,
        &(SDL_GPUBufferCreateInfo){
            .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
            .size = size,
        }
    );
}

static void upload_static_buffer(
    SDL_GPUCopyPass *copy,
    SDL_GPUTransferBuffer *transfer,
    SDL_GPUBuffer *buffer,
    Uint32 offset,
    Uint32 size
) {
    if (size == 0) {
        return;
    }
    SDL_UploadToGPUBuffer(
        copy,
        &(SDL_GPUTransferBufferLocation){
            .transfer_buffer = transfer,
            .offset = offset,
        },
        &(SDL_GPUBufferRegion){ .buffer = buffer, .size = size },
        false
    );
}

void sigpu_static_chunk_upload(sigpu_static_chunk_t *chunk, const sigpu_static_upload_t *upload) {
    Uint32 transfer_size = 0;
    for (Uint32 primitive = 0; primitive < SIGPU_PRIMITIVE_COUNT; primitive++) {
        if (chunk->axis_buffer[primitive])
            SDL_ReleaseGPUBuffer(SIGPU_GPUCONTEXT->device, chunk->axis_buffer[primitive]);
        if (chunk->rotated_buffer[primitive])
            SDL_ReleaseGPUBuffer(SIGPU_GPUCONTEXT->device, chunk->rotated_buffer[primitive]);
        Uint32 axis_size = upload->axis_count[primitive] * sizeof(sigpu_axis_instance_t);
        Uint32 rotated_size = upload->rotated_count[primitive] * sizeof(sigpu_rotated_instance_t);
        chunk->axis_buffer[primitive] = axis_size ? create_static_buffer(axis_size) : NULL;
        chunk->rotated_buffer[primitive] = rotated_size ? create_static_buffer(rotated_size) : NULL;
        chunk->axis[primitive] = (sigpu_static_range_t){ .count = upload->axis_count[primitive] };
        chunk->rotated[primitive] =
            (sigpu_static_range_t){ .count = upload->rotated_count[primitive] };
        transfer_size += axis_size + rotated_size;
    }
    if (!transfer_size)
        return;
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(
        SIGPU_GPUCONTEXT->device,
        &(SDL_GPUTransferBufferCreateInfo){ .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                            .size = transfer_size }
    );
    uint8_t *mapped = SDL_MapGPUTransferBuffer(SIGPU_GPUCONTEXT->device, transfer, false);
    Uint32 offset = 0;
    for (Uint32 primitive = 0; primitive < SIGPU_PRIMITIVE_COUNT; primitive++) {
        Uint32 axis_size = upload->axis_count[primitive] * sizeof(sigpu_axis_instance_t);
        Uint32 rotated_size = upload->rotated_count[primitive] * sizeof(sigpu_rotated_instance_t);
        if (axis_size)
            memcpy(mapped + offset, upload->axis[primitive], axis_size);
        offset += axis_size;
        if (rotated_size)
            memcpy(mapped + offset, upload->rotated[primitive], rotated_size);
        offset += rotated_size;
    }
    SDL_UnmapGPUTransferBuffer(SIGPU_GPUCONTEXT->device, transfer);
    SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(SIGPU_GPUCONTEXT->device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(command_buffer);
    offset = 0;
    for (Uint32 primitive = 0; primitive < SIGPU_PRIMITIVE_COUNT; primitive++) {
        Uint32 axis_size = upload->axis_count[primitive] * sizeof(sigpu_axis_instance_t);
        Uint32 rotated_size = upload->rotated_count[primitive] * sizeof(sigpu_rotated_instance_t);
        upload_static_buffer(copy, transfer, chunk->axis_buffer[primitive], offset, axis_size);
        offset += axis_size;
        upload_static_buffer(
            copy,
            transfer,
            chunk->rotated_buffer[primitive],
            offset,
            rotated_size
        );
        offset += rotated_size;
    }
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(command_buffer);
    SDL_ReleaseGPUTransferBuffer(SIGPU_GPUCONTEXT->device, transfer);
}

void sigpu_static_chunks_release(void) {
    for (Uint32 i = 0; i < SIGPU_STATICRENDERCACHE->static_chunk_count; i++) {
        sigpu_static_chunk_t *chunk = &SIGPU_STATICRENDERCACHE->static_chunks[i];
        for (Uint32 p = 0; p < SIGPU_PRIMITIVE_COUNT; p++) {
            if (chunk->axis_buffer[p])
                SDL_ReleaseGPUBuffer(SIGPU_GPUCONTEXT->device, chunk->axis_buffer[p]);
            if (chunk->rotated_buffer[p])
                SDL_ReleaseGPUBuffer(SIGPU_GPUCONTEXT->device, chunk->rotated_buffer[p]);
        }
    }
}

void sigpu_resources_create(int samples) {
    SIGPU_GPUTARGETS->depth_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    if (SDL_GPUTextureSupportsFormat(
            SIGPU_GPUCONTEXT->device,
            SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
        ))
        SIGPU_GPUTARGETS->depth_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    else if (
        SDL_GPUTextureSupportsFormat(
            SIGPU_GPUCONTEXT->device,
            SDL_GPU_TEXTUREFORMAT_D24_UNORM,
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
        )
    )
        SIGPU_GPUTARGETS->depth_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM;
    SIGPU_GPUTARGETS->sample_count = sigpu_supported_sample_count(samples);
    sigpu_meshes_create();
    resize_axis_instances(SIGPU_AXIS_CAPACITY);
    resize_rotated_instances(SIGPU_ROTATED_CAPACITY);
    resize_instances(&SIGPU_GPUCONTEXT->shadow_axis_buffer, &SIGPU_GPUCONTEXT->shadow_axis_transfer, 8192, sizeof(sigpu_axis_instance_t));
    resize_instances(&SIGPU_GPUCONTEXT->shadow_rotated_buffer, &SIGPU_GPUCONTEXT->shadow_rotated_transfer, 2048, sizeof(sigpu_rotated_instance_t));
    SIGPU_GPUCONTEXT->shadow_axis_capacity = 8192;
    SIGPU_GPUCONTEXT->shadow_rotated_capacity = 2048;
    sigpu_pipelines_create();
}

void sigpu_resources_destroy(void) {
    sigpu_pipelines_destroy();
    SDL_ReleaseGPUBuffer(SIGPU_GPUCONTEXT->device, SIGPU_GPUCONTEXT->vertex_buffer);
    SDL_ReleaseGPUBuffer(SIGPU_GPUCONTEXT->device, SIGPU_GPUCONTEXT->index_buffer);
    SDL_ReleaseGPUBuffer(SIGPU_GPUCONTEXT->device, SIGPU_GPUCONTEXT->axis_buffer);
    SDL_ReleaseGPUBuffer(SIGPU_GPUCONTEXT->device, SIGPU_GPUCONTEXT->rotated_buffer);
    SDL_ReleaseGPUTransferBuffer(SIGPU_GPUCONTEXT->device, SIGPU_GPUCONTEXT->axis_transfer);
    SDL_ReleaseGPUTransferBuffer(SIGPU_GPUCONTEXT->device, SIGPU_GPUCONTEXT->rotated_transfer);
    SDL_ReleaseGPUBuffer(SIGPU_GPUCONTEXT->device, SIGPU_GPUCONTEXT->shadow_axis_buffer);
    SDL_ReleaseGPUBuffer(SIGPU_GPUCONTEXT->device, SIGPU_GPUCONTEXT->shadow_rotated_buffer);
    SDL_ReleaseGPUTransferBuffer(SIGPU_GPUCONTEXT->device, SIGPU_GPUCONTEXT->shadow_axis_transfer);
    SDL_ReleaseGPUTransferBuffer(SIGPU_GPUCONTEXT->device, SIGPU_GPUCONTEXT->shadow_rotated_transfer);
    sigpu_frame_targets_release();
    sigpu_static_chunks_release();
    SDL_free(SIGPU_STATICRENDERCACHE->static_chunks);
    SDL_free(SIGPU_STATICRENDERCACHE->chunk_info);
    SDL_free(SIGPU_STATICRENDERCACHE->dirty_entities);
    SDL_free(SIGPU_STATICRENDERCACHE->dirty_roots);
}

void sigpu_axis_instances_grow(void) {
    grow_instances(
        &SIGPU_GPUCONTEXT->axis_buffer,
        &SIGPU_GPUCONTEXT->axis_transfer,
        &SIGPU_RENDERQUEUE->axis_mapped,
        &SIGPU_GPUCONTEXT->axis_capacity,
        sizeof(sigpu_shared_axis_instance_t),
        SIGPU_RENDERQUEUE->shared_axis_count,
        sizeof(sigpu_axis_instance_t),
        SIGPU_RENDERQUEUE->owned_axis_count
    );
}

void sigpu_rotated_instances_grow(void) {
    grow_instances(
        &SIGPU_GPUCONTEXT->rotated_buffer,
        &SIGPU_GPUCONTEXT->rotated_transfer,
        &SIGPU_RENDERQUEUE->rotated_mapped,
        &SIGPU_GPUCONTEXT->rotated_capacity,
        sizeof(sigpu_shared_rotated_instance_t),
        SIGPU_RENDERQUEUE->shared_rotated_count,
        sizeof(sigpu_rotated_instance_t),
        SIGPU_RENDERQUEUE->owned_rotated_count
    );
}

static void upload_instance_buffer(
    SDL_GPUCopyPass *copy,
    SDL_GPUTransferBuffer *transfer,
    SDL_GPUBuffer *buffer,
    Uint32 capacity_size,
    Uint32 shared_size,
    Uint32 shared_count,
    Uint32 owned_size,
    Uint32 owned_count
) {
    bool cycle = true;
    if (shared_count) {
        SDL_UploadToGPUBuffer(
            copy,
            &(SDL_GPUTransferBufferLocation){ .transfer_buffer = transfer },
            &(SDL_GPUBufferRegion){ .buffer = buffer, .size = shared_count * shared_size },
            cycle
        );
        cycle = false;
    }
    if (owned_count) {
        Uint32 offset = capacity_size - owned_count * owned_size;
        SDL_UploadToGPUBuffer(
            copy,
            &(SDL_GPUTransferBufferLocation){ .transfer_buffer = transfer, .offset = offset },
            &(SDL_GPUBufferRegion){ .buffer = buffer,
                                    .offset = offset,
                                    .size = owned_count * owned_size },
            cycle
        );
    }
}

void sigpu_upload_instances(void) {
    SDL_UnmapGPUTransferBuffer(SIGPU_GPUCONTEXT->device, SIGPU_GPUCONTEXT->axis_transfer);
    SDL_UnmapGPUTransferBuffer(SIGPU_GPUCONTEXT->device, SIGPU_GPUCONTEXT->rotated_transfer);
    SDL_UnmapGPUTransferBuffer(SIGPU_GPUCONTEXT->device, SIGPU_GPUCONTEXT->shadow_axis_transfer);
    SDL_UnmapGPUTransferBuffer(SIGPU_GPUCONTEXT->device, SIGPU_GPUCONTEXT->shadow_rotated_transfer);
    if (!SIGPU_FRAMECONTEXT->swapchain)
        return;
    if (!SIGPU_RENDERQUEUE->shared_axis_count && !SIGPU_RENDERQUEUE->shared_rotated_count &&
        !SIGPU_RENDERQUEUE->owned_axis_count && !SIGPU_RENDERQUEUE->owned_rotated_count &&
        !SIGPU_RENDERQUEUE->shadow_shared_axis_count && !SIGPU_RENDERQUEUE->shadow_shared_rotated_count &&
        !SIGPU_RENDERQUEUE->shadow_owned_axis_count && !SIGPU_RENDERQUEUE->shadow_owned_rotated_count)
        return;
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(SIGPU_FRAMECONTEXT->command_buffer);
    upload_instance_buffer(
        copy,
        SIGPU_GPUCONTEXT->axis_transfer,
        SIGPU_GPUCONTEXT->axis_buffer,
        SIGPU_GPUCONTEXT->axis_capacity * sizeof(sigpu_axis_instance_t),
        sizeof(sigpu_shared_axis_instance_t),
        SIGPU_RENDERQUEUE->shared_axis_count,
        sizeof(sigpu_axis_instance_t),
        SIGPU_RENDERQUEUE->owned_axis_count
    );
    upload_instance_buffer(
        copy,
        SIGPU_GPUCONTEXT->rotated_transfer,
        SIGPU_GPUCONTEXT->rotated_buffer,
        SIGPU_GPUCONTEXT->rotated_capacity * sizeof(sigpu_rotated_instance_t),
        sizeof(sigpu_shared_rotated_instance_t),
        SIGPU_RENDERQUEUE->shared_rotated_count,
        sizeof(sigpu_rotated_instance_t),
        SIGPU_RENDERQUEUE->owned_rotated_count
    );
    upload_instance_buffer(copy, SIGPU_GPUCONTEXT->shadow_axis_transfer,
        SIGPU_GPUCONTEXT->shadow_axis_buffer,
        SIGPU_GPUCONTEXT->shadow_axis_capacity * sizeof(sigpu_axis_instance_t),
        sizeof(sigpu_shared_axis_instance_t), SIGPU_RENDERQUEUE->shadow_shared_axis_count,
        sizeof(sigpu_axis_instance_t), SIGPU_RENDERQUEUE->shadow_owned_axis_count);
    upload_instance_buffer(copy, SIGPU_GPUCONTEXT->shadow_rotated_transfer,
        SIGPU_GPUCONTEXT->shadow_rotated_buffer,
        SIGPU_GPUCONTEXT->shadow_rotated_capacity * sizeof(sigpu_rotated_instance_t),
        sizeof(sigpu_shared_rotated_instance_t), SIGPU_RENDERQUEUE->shadow_shared_rotated_count,
        sizeof(sigpu_rotated_instance_t), SIGPU_RENDERQUEUE->shadow_owned_rotated_count);
    SDL_EndGPUCopyPass(copy);
}

void sigpu_instances_reserve(Uint32 additional) {
    while (SIGPU_RENDERQUEUE->shared_axis_count + SIGPU_RENDERQUEUE->owned_axis_count + additional >
           SIGPU_GPUCONTEXT->axis_capacity)
        sigpu_axis_instances_grow();
    while (SIGPU_RENDERQUEUE->shared_rotated_count + SIGPU_RENDERQUEUE->owned_rotated_count +
               additional >
           SIGPU_GPUCONTEXT->rotated_capacity)
        sigpu_rotated_instances_grow();
}

void sigpu_shadow_instances_reserve(Uint32 additional) {
    while (SIGPU_RENDERQUEUE->shadow_shared_axis_count + SIGPU_RENDERQUEUE->shadow_owned_axis_count + additional > SIGPU_GPUCONTEXT->shadow_axis_capacity)
        grow_instances(&SIGPU_GPUCONTEXT->shadow_axis_buffer, &SIGPU_GPUCONTEXT->shadow_axis_transfer,
            &SIGPU_RENDERQUEUE->shadow_axis_mapped, &SIGPU_GPUCONTEXT->shadow_axis_capacity,
            sizeof(sigpu_shared_axis_instance_t), SIGPU_RENDERQUEUE->shadow_shared_axis_count,
            sizeof(sigpu_axis_instance_t), SIGPU_RENDERQUEUE->shadow_owned_axis_count);
    while (SIGPU_RENDERQUEUE->shadow_shared_rotated_count + SIGPU_RENDERQUEUE->shadow_owned_rotated_count + additional > SIGPU_GPUCONTEXT->shadow_rotated_capacity)
        grow_instances(&SIGPU_GPUCONTEXT->shadow_rotated_buffer, &SIGPU_GPUCONTEXT->shadow_rotated_transfer,
            &SIGPU_RENDERQUEUE->shadow_rotated_mapped, &SIGPU_GPUCONTEXT->shadow_rotated_capacity,
            sizeof(sigpu_shared_rotated_instance_t), SIGPU_RENDERQUEUE->shadow_shared_rotated_count,
            sizeof(sigpu_rotated_instance_t), SIGPU_RENDERQUEUE->shadow_owned_rotated_count);
}
