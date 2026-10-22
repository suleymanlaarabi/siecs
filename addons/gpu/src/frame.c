#include "sigpu_internal.h"
#include <stdlib.h>
#include <string.h>

/* size > 0 and [offset, offset + size) inside total. */
static bool range_valid(uint32_t offset, uint32_t size, uint32_t total) {
    return size > 0 && offset <= total && size <= total - offset;
}

static bool upload_push(gpu_upload_t *upload, const void *data) {
    void *copy = malloc(upload->size);
    if (!copy) {
        return false;
    }
    memcpy(copy, data, upload->size);
    upload->data = copy;
    upload->owned = true;
    if (!gpu_vec_push(gpu_session.uploads, *upload)) {
        free(copy);
        return false;
    }
    return true;
}

bool gpu_upload_raw(SDL_GPUBuffer *buffer, uint32_t offset, const void *data, uint32_t size) {
    if (!buffer || !size) {
        return true;
    }
    const gpu_upload_t upload = { .kind = GPU_UPLOAD_RAW, .raw = buffer, .offset = offset,
                                  .data = data, .size = size };
    return gpu_vec_push(gpu_session.uploads, upload);
}

void gpu_uploads_drop_raw(SDL_GPUBuffer *buffer) {
    uint32_t kept = 0;
    for (uint32_t i = 0; i < gpu_session.uploads.count; i++) {
        const gpu_upload_t *upload = &gpu_session.uploads.items[i];
        if (upload->kind != GPU_UPLOAD_RAW || upload->raw != buffer) {
            gpu_session.uploads.items[kept++] = *upload;
        }
    }
    gpu_session.uploads.count = kept;
}

bool sigpu_buffer_upload(ecs_entity_t buffer, uint32_t offset, const void *data, uint32_t size) {
    if (!gpu_session.open || !data || !ecs_is_alive(buffer)) {
        return false;
    }
    const GpuBuffer *desc = ecs_try_get(buffer, GpuBuffer);
    if (!desc || !range_valid(offset, size, desc->size)) {
        return false;
    }
    const bool stream = desc->policy == GpuUploadStream;
    if (stream && (offset || size != desc->size)) {
        return false;
    }
    gpu_upload_t upload = { .entity = buffer, .kind = GPU_UPLOAD_BUFFER, .offset = offset,
                            .size = size, .cycle = stream };
    return upload_push(&upload, data);
}

bool sigpu_texture_upload(
    ecs_entity_t texture,
    const GpuTextureRegion *region,
    const void *data,
    size_t size,
    uint32_t pitch
) {
    if (!gpu_session.open || !region || !data || !ecs_is_alive(texture)) {
        return false;
    }
    const GpuTexture *desc = ecs_try_get(texture, GpuTexture);
    const uint32_t texel = desc ? gpu_format_size(desc->format) : 0;
    if (!texel || !region->width || !region->height || !region->depth) {
        return false;
    }
    const uint64_t row = (uint64_t)region->width * texel;
    pitch = pitch ? pitch : (uint32_t)row;
    if (row > UINT32_MAX || pitch % texel || pitch < row) {
        return false;
    }
    const uint64_t rows = (uint64_t)region->height * region->depth;
    const uint64_t needed = (uint64_t)pitch * (rows - 1) + row;
    if (needed > size || needed > UINT32_MAX) {
        return false;
    }
    gpu_upload_t upload = { .entity = texture, .kind = GPU_UPLOAD_TEXTURE, .region = *region,
                            .pitch = pitch, .size = (uint32_t)needed };
    return upload_push(&upload, data);
}

static uint32_t mip_size(uint32_t size, uint32_t mip) {
    const uint32_t scaled = mip < 32 ? size >> mip : 0;
    return scaled ? scaled : 1;
}

static bool region_valid(const gpu_texture_info_t *info, const GpuTextureRegion *region) {
    const bool volume = info->dimension == GpuTexture3d;
    const uint32_t layers = volume ? 1 : info->depth;
    const uint32_t depth = volume ? mip_size(info->depth, region->mip) : 1;
    return region->mip < info->mips && region->layer < layers &&
           range_valid(region->x, region->width, mip_size(info->width, region->mip)) &&
           range_valid(region->y, region->height, mip_size(info->height, region->mip)) &&
           range_valid(region->z, region->depth, depth);
}

/* Staging bytes of an upload: a texture upload stages every row at its
 * pitch, the last one included. Offsets stay 16-aligned, enough for any
 * texel size. */
static uint64_t staged_size(const gpu_upload_t *upload) {
    const uint64_t size = upload->kind == GPU_UPLOAD_TEXTURE
                              ? (uint64_t)upload->pitch * upload->region.height *
                                    upload->region.depth
                              : upload->size;
    return (size + 15u) & ~(uint64_t)15u;
}

static bool upload_fits(const gpu_upload_t *upload) {
    return upload->kind == GPU_UPLOAD_TEXTURE
               ? region_valid(&upload->record->as.texture.info, &upload->region)
               : range_valid(upload->offset, upload->size, upload->record->as.buffer.size);
}

/*
 * Resolves each upload's target once: record is set for the ones to apply
 * now and NULL for the ones whose target is Pending or Waiting; the others
 * are dropped. Returns the ready count and their staging bytes.
 */
static uint32_t uploads_resolve(uint64_t *bytes) {
    uint32_t ready = 0, kept = 0;
    *bytes = 0;
    for (uint32_t i = 0; i < gpu_session.uploads.count; i++) {
        gpu_upload_t upload = gpu_session.uploads.items[i];
        if (upload.kind == GPU_UPLOAD_RAW) {
            ready++;
            *bytes += staged_size(&upload);
            gpu_session.uploads.items[kept++] = upload;
            continue;
        }
        const GpuResourceState *state = gpu_state(upload.entity);
        upload.record = gpu_resource_record(
            upload.entity,
            upload.kind == GPU_UPLOAD_TEXTURE ? GPU_RESOURCE_TEXTURE : GPU_RESOURCE_BUFFER
        );
        const bool fits = upload.record && upload_fits(&upload) &&
                          (!upload.cycle || (upload.kind == GPU_UPLOAD_BUFFER &&
                                             upload.record->as.buffer.policy == GpuUploadStream &&
                                             upload.size == upload.record->as.buffer.size));
        const bool waits = !upload.record && state &&
                           (state->status == GpuPending || state->status == GpuWaiting);
        if (!fits && !waits) {
            if (upload.record) {
                SDL_LogError(SDL_LOG_CATEGORY_GPU, "sigpu: upload to %s dropped: out of range",
                             ecs_entity_name(upload.entity));
            }
            free((void *)upload.data);
            continue;
        }
        if (fits) {
            ready++;
            *bytes += staged_size(&upload);
        } else {
            upload.record = NULL;
        }
        gpu_session.uploads.items[kept++] = upload;
    }
    gpu_session.uploads.count = kept;
    return ready;
}

static void upload_record(SDL_GPUCopyPass *copy, SDL_GPUTransferBuffer *staging, uint32_t offset,
                          const gpu_upload_t *upload) {
    if (upload->kind != GPU_UPLOAD_TEXTURE) {
        SDL_UploadToGPUBuffer(
            copy,
            &(SDL_GPUTransferBufferLocation){ .transfer_buffer = staging, .offset = offset },
            &(SDL_GPUBufferRegion){ .buffer = upload->kind == GPU_UPLOAD_RAW
                                                  ? upload->raw
                                                  : upload->record->as.buffer.buffer,
                                    .offset = upload->offset,
                                    .size = upload->size },
            upload->cycle
        );
        return;
    }
    const GpuTextureRegion *region = &upload->region;
    SDL_UploadToGPUTexture(
        copy,
        &(SDL_GPUTextureTransferInfo){
            .transfer_buffer = staging,
            .offset = offset,
            .pixels_per_row =
                upload->pitch / gpu_format_size(upload->record->as.texture.info.format),
            .rows_per_layer = region->height },
        &(SDL_GPUTextureRegion){ .texture = upload->record->as.texture.texture,
                                 .mip_level = region->mip, .layer = region->layer,
                                 .x = region->x, .y = region->y, .z = region->z,
                                 .w = region->width, .h = region->height, .d = region->depth },
        false
    );
}

/* Before recording submission k, submission k - frames_in_flight must have
 * completed: the CPU never runs further ahead. */
static bool wait_frame_slot(void) {
    SDL_GPUFence **fence =
        &gpu_session.fences[gpu_session.submissions % gpu_session.frames_in_flight];
    if (!*fence) {
        return true;
    }
    if (!SDL_QueryGPUFence(gpu_session.gpu, *fence)) {
        ecs_get_resource(GpuStats)->frame_waits++;
        if (!SDL_WaitForGPUFences(gpu_session.gpu, true, fence, 1)) {
            gpu_mark_lost("fence wait");
            return false;
        }
    }
    SDL_ReleaseGPUFence(gpu_session.gpu, *fence);
    *fence = NULL;
    return true;
}

/* The staging of the current frame slot, grown to size. Its previous
 * submission completed (wait_frame_slot), so it is free to map. */
static SDL_GPUTransferBuffer *staging_get(uint32_t size) {
    const uint32_t slot = gpu_session.submissions % gpu_session.frames_in_flight;
    if (gpu_session.staging_size[slot] < size) {
        if (gpu_session.staging[slot]) {
            SDL_ReleaseGPUTransferBuffer(gpu_session.gpu, gpu_session.staging[slot]);
            gpu_counted_release(gpu_session.staging[slot]);
            gpu_stats()->memory_reserved -= gpu_session.staging_size[slot];
        }
        uint32_t capacity = 64u * 1024u;
        while (capacity < size && capacity < 0x80000000u) {
            capacity *= 2;
        }
        capacity = capacity < size ? size : capacity;
        gpu_session.staging[slot] = SDL_CreateGPUTransferBuffer(
            gpu_session.gpu,
            &(SDL_GPUTransferBufferCreateInfo){ .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                                .size = capacity }
        );
        gpu_counted_create(gpu_session.staging[slot], "staging buffer");
        gpu_session.staging_size[slot] = gpu_session.staging[slot] ? capacity : 0;
        gpu_stats()->memory_reserved += gpu_session.staging_size[slot];
    }
    return gpu_session.staging[slot];
}

void gpu_staging_release(void) {
    for (uint32_t i = 0; i < GPU_MAX_FRAMES_IN_FLIGHT; i++) {
        if (gpu_session.staging[i]) {
            SDL_ReleaseGPUTransferBuffer(gpu_session.gpu, gpu_session.staging[i]);
            gpu_counted_release(gpu_session.staging[i]);
            gpu_stats()->memory_reserved -= gpu_session.staging_size[i];
        }
        gpu_session.staging[i] = NULL;
        gpu_session.staging_size[i] = 0;
    }
}

/* Copies the ready uploads into the frame's staging and records them. */
static bool uploads_record(SDL_GPUCommandBuffer *cmd, uint32_t ready, uint64_t bytes) {
    SDL_GPUDevice *gpu = gpu_session.gpu;
    SDL_GPUTransferBuffer *staging = staging_get((uint32_t)bytes);
    uint8_t *mapped = staging ? SDL_MapGPUTransferBuffer(gpu, staging, false) : NULL;
    if (!mapped) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "sigpu: upload staging: %s", SDL_GetError());
        return false;
    }
    gpu_upload_t *uploads = gpu_session.uploads.items;
    uint32_t offset = 0;
    for (uint32_t i = 0; i < gpu_session.uploads.count; i++) {
        if (uploads[i].record || uploads[i].kind == GPU_UPLOAD_RAW) {
            memcpy(mapped + offset, uploads[i].data, uploads[i].size);
            offset += (uint32_t)staged_size(&uploads[i]);
        }
    }
    SDL_UnmapGPUTransferBuffer(gpu, staging);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    GpuStats *counters = gpu_stats();
    uint32_t kept = 0;
    offset = 0;
    for (uint32_t i = 0; i < gpu_session.uploads.count; i++) {
        if (!uploads[i].record && uploads[i].kind != GPU_UPLOAD_RAW) {
            uploads[kept++] = uploads[i];
            continue;
        }
        upload_record(copy, staging, offset, &uploads[i]);
        offset += (uint32_t)staged_size(&uploads[i]);
        counters->upload_bytes += uploads[i].size;
        if (uploads[i].owned) {
            free((void *)uploads[i].data);
        }
    }
    gpu_session.uploads.count = kept;
    counters->uploads += ready;
    SDL_EndGPUCopyPass(copy);
    return true;
}

static bool graphs_pending(void) {
    for (uint32_t g = 0; g < gpu_session.graphs.count; g++) {
        if (gpu_session.graphs.items[g].valid && gpu_session.graphs.items[g].plan.count) {
            return true;
        }
    }
    return false;
}

void gpu_prepare(ecs_iter_t *it) {
    (void)it;
    if (!gpu_session.gpu || gpu_session.lost) {
        return;
    }
    const uint64_t start = SDL_GetTicksNS();
    gpu_materials_prepare();
    gpu_features_prepare();
    gpu_graphs_prepare();
    gpu_batches_prepare_all();
    gpu_session.prewarm = false;
    gpu_stats()->prepare_ns = SDL_GetTicksNS() - start;
}

void gpu_record(ecs_iter_t *it) {
    (void)it;
    if (!gpu_session.gpu || gpu_session.lost) {
        return;
    }
    const uint64_t start = SDL_GetTicksNS();
    uint64_t bytes = 0;
    const uint32_t ready = gpu_session.uploads.count ? uploads_resolve(&bytes) : 0;
    const bool graphs = graphs_pending();
    if (!ready && !graphs) {
        return;
    }
    if (bytes > UINT32_MAX) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "sigpu: more than 4 GiB of uploads in one frame");
        return;
    }
    if (!wait_frame_slot()) {
        return;
    }
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gpu_session.gpu);
    if (!cmd) {
        gpu_mark_lost("command buffer acquisition");
        return;
    }
    if (ready) {
        uploads_record(cmd, ready, bytes);
    }
    if (graphs) {
        gpu_graphs_record(cmd);
    }
    gpu_session.cmd = cmd;
    gpu_stats()->record_ns = SDL_GetTicksNS() - start;
}

static void capture_from_environment(void);

void gpu_submit(ecs_iter_t *it) {
    (void)it;
    if (!gpu_session.cmd) {
        gpu_surfaces_frame_end();
        return;
    }
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(gpu_session.cmd);
    gpu_session.cmd = NULL;
    gpu_surfaces_frame_end();
    if (!fence) {
        gpu_mark_lost("submission");
        return;
    }
    gpu_session.fences[gpu_session.submissions % gpu_session.frames_in_flight] = fence;
    gpu_session.submissions++;
    gpu_stats()->submissions++;
    ecs_get_resource(GpuSession)->submissions = gpu_session.submissions;
    capture_from_environment();
}

bool sigpu_capture_bmp(ecs_entity_t resource, const char *path) {
    const GpuSurfaceTargets *targets =
        ecs_is_alive(resource) ? ecs_try_get(resource, GpuSurfaceTargets) : NULL;
    gpu_texture_info_t info;
    SDL_GPUTexture *texture = gpu_resource_texture(targets ? targets->color : resource, &info);
    SDL_GPUDevice *gpu = gpu_session.gpu;
    if (!texture || !path || (info.format != GpuFormatRgba8 && info.format != GpuFormatRgba8Srgb)) {
        return false;
    }
    const uint32_t size = info.width * info.height * 4;
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(
        gpu, &(SDL_GPUTransferBufferCreateInfo){ .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                                                 .size = size });
    SDL_GPUCommandBuffer *cmd = transfer ? SDL_AcquireGPUCommandBuffer(gpu) : NULL;
    if (!cmd) {
        SDL_ReleaseGPUTransferBuffer(gpu, transfer);
        return false;
    }
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_DownloadFromGPUTexture(copy,
                               &(SDL_GPUTextureRegion){ .texture = texture, .w = info.width,
                                                        .h = info.height, .d = 1 },
                               &(SDL_GPUTextureTransferInfo){ .transfer_buffer = transfer });
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    const bool done = fence && SDL_WaitForGPUFences(gpu, true, &fence, 1);
    SDL_ReleaseGPUFence(gpu, fence);
    bool saved = false;
    void *mapped = done ? SDL_MapGPUTransferBuffer(gpu, transfer, false) : NULL;
    if (mapped) {
        SDL_Surface *surface = SDL_CreateSurfaceFrom((int)info.width, (int)info.height,
                                                     SDL_PIXELFORMAT_ABGR8888, mapped,
                                                     (int)info.width * 4);
        saved = surface && SDL_SaveBMP(surface, path);
        SDL_DestroySurface(surface);
        SDL_UnmapGPUTransferBuffer(gpu, transfer);
    }
    SDL_ReleaseGPUTransferBuffer(gpu, transfer);
    return saved;
}

/* SIGPU_CAPTURE: saves the surfaces once, after the configured frame. */
static void capture_from_environment(void) {
    const char *path = SDL_getenv("SIGPU_CAPTURE");
    if (!path || !path[0]) {
        return;
    }
    const char *frame = SDL_getenv("SIGPU_CAPTURE_FRAME");
    const uint64_t at = frame ? (uint64_t)SDL_strtoull(frame, NULL, 10) : 60;
    if (gpu_session.frame != at) {
        return;
    }
    uint32_t index = 0;
    ecs_query_each(it, i, ecs_filter(GpuSurface)) {
        char name[512];
        if (index) {
            SDL_snprintf(name, sizeof(name), "%s.%u.bmp", path, index);
        } else {
            SDL_strlcpy(name, path, sizeof(name));
        }
        SDL_Log("sigpu: capture %s: %s", name,
                sigpu_capture_bmp(it.entities[i], name) ? "saved" : "failed");
        index++;
    }
    if (SDL_getenv("SIGPU_CAPTURE_QUIT")) {
        ecs_quit();
    }
}
