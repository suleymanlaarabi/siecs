/*
 * M3 proof for sigpu v2 (RFC section 16): a Slang pack built by
 * addons/gpu/tools/gpushader, a compute dispatch that writes instances and
 * visible indices, and an instanced draw that reads them through storage
 * buffers in the vertex and fragment stages.
 *
 * The second draw starts at first_instance = PROOF_HALF, so the bottom half
 * of the grid only appears when first_instance offsets the per-instance
 * vertex buffer.
 *
 * Headless by default: renders offscreen, reads the pixels back and prints a
 * verdict per point. --window shows the same image in a siwindow window.
 */
#include "backend/gpushader.h"
#include "proof_pack.generated.h"

#include <SDL3/SDL.h>
#include <siwindow/native.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define PROOF_COLUMNS 8u
#define PROOF_COUNT 64u
#define PROOF_VISIBLE (PROOF_COUNT / 2u)
#define PROOF_HALF (PROOF_VISIBLE / 2u)
#define PROOF_CELL 8.0f
#define PROOF_MARGIN 1.0f
#define PROOF_SIZE 64u

/* CPU side of the pack's interface, checked against its reflection. */
typedef struct {
    float position[2];
    float size[2];
    uint32_t material;
    uint32_t padding;
} Instance;

typedef struct {
    float color[4];
} Material;

typedef struct {
    uint32_t columns;
    uint32_t count;
    float cell;
    float margin;
} Grid;

typedef struct {
    float target_size[2];
} View;

typedef struct {
    gpu_pack_t pack;
    const gpu_pack_entry_t *build, *vertex, *fragment;
    SDL_GPUDevice *device;
    SDL_GPUComputePipeline *compute;
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_GPUBuffer *instances, *visible, *materials, *quad;
    SDL_GPUTexture *target;
} Proof;

static int error_logs;
static SDL_LogOutputFunction default_log;
static void *default_log_data;

/* Counts SDL errors, then logs as usual. Vulkan validation messages bypass
 * SDL_Log and go straight to stdout. */
static void count_errors(void *data, int category, SDL_LogPriority priority, const char *message) {
    (void)data;
    if (priority >= SDL_LOG_PRIORITY_ERROR) {
        error_logs++;
    }
    default_log(default_log_data, category, priority, message);
}

static bool fail(const char *what) {
    fprintf(stderr, "gpu_proof: %s: %s\n", what, SDL_GetError());
    return false;
}

static bool field_matches(
    const Proof *proof,
    const gpu_pack_resource_t *resource,
    const char *name,
    size_t offset,
    size_t size
) {
    const gpu_pack_field_t *field = gpu_pack_field(&proof->pack, resource, name);
    if (field && field->offset == offset && field->size == size) {
        return true;
    }
    fprintf(stderr, "gpu_proof: %s.%s does not match the C layout\n", resource->name, name);
    return false;
}

#define FIELD_MATCHES(proof, resource, type, field)                                                \
    field_matches(proof, resource, #field, offsetof(type, field), sizeof(((type *)0)->field))

/* The future packing plan (RFC 10.3) in miniature: direct copies are only
 * used when the C layout equals the reflected one. */
static bool check_layouts(const Proof *proof) {
    const gpu_pack_resource_t *instances = gpu_pack_resource(&proof->pack, proof->vertex, "instances");
    const gpu_pack_resource_t *out = gpu_pack_resource(&proof->pack, proof->build, "out_instances");
    const gpu_pack_resource_t *materials =
        gpu_pack_resource(&proof->pack, proof->fragment, "materials");
    const gpu_pack_resource_t *grid = gpu_pack_resource(&proof->pack, proof->build, "grid");
    const gpu_pack_resource_t *view = gpu_pack_resource(&proof->pack, proof->vertex, "view");
    if (!instances || !out || !materials || !grid || !view) {
        fprintf(stderr, "gpu_proof: the pack does not declare the expected resources\n");
        return false;
    }
    bool ok = instances->stride == sizeof(Instance) && out->stride == sizeof(Instance) &&
              materials->stride == sizeof(Material) && grid->size == sizeof(Grid) &&
              view->size >= sizeof(View);
    ok = ok && FIELD_MATCHES(proof, instances, Instance, position) &&
         FIELD_MATCHES(proof, instances, Instance, size) &&
         FIELD_MATCHES(proof, instances, Instance, material) &&
         FIELD_MATCHES(proof, materials, Material, color) &&
         FIELD_MATCHES(proof, grid, Grid, columns) && FIELD_MATCHES(proof, grid, Grid, count) &&
         FIELD_MATCHES(proof, grid, Grid, cell) && FIELD_MATCHES(proof, grid, Grid, margin) &&
         FIELD_MATCHES(proof, view, View, target_size);
    if (!ok) {
        fprintf(stderr, "gpu_proof: C and Slang layouts differ\n");
    }
    return ok;
}

static uint32_t input_location(const Proof *proof, const char *name) {
    for (uint32_t i = 0; i < proof->vertex->input_count; i++) {
        const gpu_pack_input_t *input = &proof->pack.inputs[proof->vertex->first_input + i];
        if (strcmp(input->name, name) == 0) {
            return input->location;
        }
    }
    return UINT32_MAX;
}

static bool upload(Proof *proof, SDL_GPUBuffer *buffer, const void *data, uint32_t size) {
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(
        proof->device,
        &(SDL_GPUTransferBufferCreateInfo){ .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                            .size = size }
    );
    if (!transfer) {
        return fail("transfer buffer");
    }
    memcpy(SDL_MapGPUTransferBuffer(proof->device, transfer, false), data, size);
    SDL_UnmapGPUTransferBuffer(proof->device, transfer);
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(proof->device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_UploadToGPUBuffer(
        copy,
        &(SDL_GPUTransferBufferLocation){ .transfer_buffer = transfer },
        &(SDL_GPUBufferRegion){ .buffer = buffer, .size = size },
        false
    );
    SDL_EndGPUCopyPass(copy);
    const bool submitted = SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(proof->device, transfer);
    return submitted || fail("upload");
}

static SDL_GPUBuffer *create_buffer(Proof *proof, SDL_GPUBufferUsageFlags usage, uint32_t size) {
    SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(
        proof->device,
        &(SDL_GPUBufferCreateInfo){ .usage = usage, .size = size }
    );
    if (!buffer) {
        fail("buffer");
    }
    return buffer;
}

static bool create_pipelines(Proof *proof) {
    proof->compute = gpu_pack_create_compute_pipeline(proof->device, &proof->pack, proof->build);
    SDL_GPUShader *vertex = gpu_pack_create_shader(proof->device, &proof->pack, proof->vertex);
    SDL_GPUShader *fragment = gpu_pack_create_shader(proof->device, &proof->pack, proof->fragment);
    const uint32_t corner = input_location(proof, "corner");
    const uint32_t visible = input_location(proof, "visible");
    bool ok = proof->compute && vertex && fragment && corner != UINT32_MAX &&
              visible != UINT32_MAX;
    if (ok) {
        SDL_GPUVertexBufferDescription buffers[] = {
            { .slot = 0, .pitch = sizeof(float[2]), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX },
            { .slot = 1, .pitch = sizeof(uint32_t), .input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE },
        };
        SDL_GPUVertexAttribute attributes[] = {
            { .location = corner, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2 },
            { .location = visible, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT },
        };
        SDL_GPUColorTargetDescription color = { .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM };
        proof->pipeline = SDL_CreateGPUGraphicsPipeline(
            proof->device,
            &(SDL_GPUGraphicsPipelineCreateInfo){
                .vertex_shader = vertex,
                .fragment_shader = fragment,
                .vertex_input_state = { .vertex_buffer_descriptions = buffers,
                                        .num_vertex_buffers = 2,
                                        .vertex_attributes = attributes,
                                        .num_vertex_attributes = 2 },
                .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
                .target_info = { .color_target_descriptions = &color,
                                 .num_color_targets = 1 },
            }
        );
        ok = proof->pipeline || fail("graphics pipeline");
    }
    if (vertex) {
        SDL_ReleaseGPUShader(proof->device, vertex);
    }
    if (fragment) {
        SDL_ReleaseGPUShader(proof->device, fragment);
    }
    return ok;
}

static bool create_resources(Proof *proof) {
    proof->instances = create_buffer(
        proof,
        SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,
        PROOF_COUNT * sizeof(Instance)
    );
    proof->visible = create_buffer(
        proof,
        SDL_GPU_BUFFERUSAGE_VERTEX | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE,
        PROOF_VISIBLE * sizeof(uint32_t)
    );
    proof->materials = create_buffer(
        proof,
        SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
        2 * sizeof(Material)
    );
    proof->quad = create_buffer(proof, SDL_GPU_BUFFERUSAGE_VERTEX, 6 * sizeof(float[2]));
    proof->target = SDL_CreateGPUTexture(
        proof->device,
        &(SDL_GPUTextureCreateInfo){
            .type = SDL_GPU_TEXTURETYPE_2D,
            .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
            .width = PROOF_SIZE,
            .height = PROOF_SIZE,
            .layer_count_or_depth = 1,
            .num_levels = 1,
        }
    );
    if (!proof->instances || !proof->visible || !proof->materials || !proof->quad ||
        !proof->target) {
        return fail("resources");
    }
    const Material materials[2] = { { { 1.0f, 0.0f, 0.0f, 1.0f } },
                                    { { 0.0f, 1.0f, 0.0f, 1.0f } } };
    const float quad[6][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 0, 1 }, { 1, 0 }, { 1, 1 } };
    return upload(proof, proof->materials, materials, sizeof(materials)) &&
           upload(proof, proof->quad, quad, sizeof(quad));
}

static void record(Proof *proof, SDL_GPUCommandBuffer *cmd) {
    const gpu_pack_resource_t *out_instances =
        gpu_pack_resource(&proof->pack, proof->build, "out_instances");
    const gpu_pack_resource_t *out_visible =
        gpu_pack_resource(&proof->pack, proof->build, "out_visible");
    SDL_GPUStorageBufferReadWriteBinding writes[2];
    writes[out_instances->slot] = (SDL_GPUStorageBufferReadWriteBinding){ .buffer =
                                                                              proof->instances };
    writes[out_visible->slot] = (SDL_GPUStorageBufferReadWriteBinding){ .buffer = proof->visible };
    SDL_GPUComputePass *compute = SDL_BeginGPUComputePass(cmd, NULL, 0, writes, 2);
    SDL_BindGPUComputePipeline(compute, proof->compute);
    const Grid grid = { PROOF_COLUMNS, PROOF_COUNT, PROOF_CELL, PROOF_MARGIN };
    const gpu_pack_resource_t *grid_resource = gpu_pack_resource(&proof->pack, proof->build, "grid");
    SDL_PushGPUComputeUniformData(cmd, grid_resource->slot, &grid, sizeof(grid));
    const uint32_t threads = proof->build->threads[0];
    SDL_DispatchGPUCompute(compute, (PROOF_COUNT + threads - 1) / threads, 1, 1);
    SDL_EndGPUComputePass(compute);

    SDL_GPUColorTargetInfo color = {
        .texture = proof->target,
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
        .clear_color = { 0.0f, 0.0f, 0.0f, 1.0f },
    };
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color, 1, NULL);
    SDL_BindGPUGraphicsPipeline(pass, proof->pipeline);
    SDL_GPUBufferBinding vertex_buffers[] = { { .buffer = proof->quad },
                                              { .buffer = proof->visible } };
    SDL_BindGPUVertexBuffers(pass, 0, vertex_buffers, 2);
    SDL_BindGPUVertexStorageBuffers(
        pass,
        gpu_pack_resource(&proof->pack, proof->vertex, "instances")->slot,
        &proof->instances,
        1
    );
    SDL_BindGPUFragmentStorageBuffers(
        pass,
        gpu_pack_resource(&proof->pack, proof->fragment, "materials")->slot,
        &proof->materials,
        1
    );
    /* Uniform blocks are pushed at their reflected size (std140 padding). */
    const gpu_pack_resource_t *view_resource =
        gpu_pack_resource(&proof->pack, proof->vertex, "view");
    uint8_t view[64] = { 0 };
    const View values = { { (float)PROOF_SIZE, (float)PROOF_SIZE } };
    memcpy(view, &values, sizeof(values));
    SDL_PushGPUVertexUniformData(cmd, view_resource->slot, view, view_resource->size);
    SDL_DrawGPUPrimitives(pass, 6, PROOF_HALF, 0, 0);
    SDL_DrawGPUPrimitives(pass, 6, PROOF_HALF, 0, PROOF_HALF);
    SDL_EndGPURenderPass(pass);
}

static bool pixel_is(const uint8_t *pixels, uint32_t x, uint32_t y, const uint8_t rgba[4]) {
    return memcmp(pixels + (y * PROOF_SIZE + x) * 4, rgba, 4) == 0;
}

/* Renders once, reads the target back and prints one verdict per point. */
static bool run_headless(Proof *proof) {
    const uint32_t size = PROOF_SIZE * PROOF_SIZE * 4;
    SDL_GPUTransferBuffer *download = SDL_CreateGPUTransferBuffer(
        proof->device,
        &(SDL_GPUTransferBufferCreateInfo){ .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                                            .size = size }
    );
    if (!download) {
        return fail("download buffer");
    }
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(proof->device);
    record(proof, cmd);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_DownloadFromGPUTexture(
        copy,
        &(SDL_GPUTextureRegion){ .texture = proof->target, .w = PROOF_SIZE, .h = PROOF_SIZE, .d = 1 },
        &(SDL_GPUTextureTransferInfo){ .transfer_buffer = download }
    );
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (!fence || !SDL_WaitForGPUFences(proof->device, true, &fence, 1)) {
        SDL_ReleaseGPUTransferBuffer(proof->device, download);
        return fail("submit");
    }
    SDL_ReleaseGPUFence(proof->device, fence);

    const uint8_t *pixels = SDL_MapGPUTransferBuffer(proof->device, download, false);
    static const uint8_t colors[2][4] = { { 255, 0, 0, 255 }, { 0, 255, 0, 255 } };
    static const uint8_t black[4] = { 0, 0, 0, 255 };
    uint32_t drawn = 0, placed = 0, colored = 0, hidden = 0, top = 0, bottom = 0;
    for (uint32_t i = 0; i < PROOF_COUNT; i++) {
        const uint32_t row = i / PROOF_COLUMNS;
        const uint32_t x = (i % PROOF_COLUMNS) * (uint32_t)PROOF_CELL;
        const uint32_t y = row * (uint32_t)PROOF_CELL;
        const uint32_t center_x = x + (uint32_t)PROOF_CELL / 2, center_y = y + (uint32_t)PROOF_CELL / 2;
        const bool lit = !pixel_is(pixels, center_x, center_y, black);
        if (i % 2) {
            hidden += !lit;
            continue;
        }
        drawn += lit;
        /* The margin stays clear only if position and size were read from
         * the instance buffer. */
        placed += lit && pixel_is(pixels, x, y, black);
        colored += pixel_is(pixels, center_x, center_y, colors[row % 2]);
        if (lit) {
            top += row < PROOF_COUNT / PROOF_COLUMNS / 2;
            bottom += row >= PROOF_COUNT / PROOF_COLUMNS / 2;
        }
    }
    SDL_UnmapGPUTransferBuffer(proof->device, download);
    SDL_ReleaseGPUTransferBuffer(proof->device, download);

    const bool dispatch = drawn > 0 && hidden == PROOF_COUNT - PROOF_VISIBLE;
    const bool vertex_storage = placed == drawn && drawn > 0;
    const bool fragment_storage = colored == drawn && drawn > 0;
    const bool first_instance = top == PROOF_HALF && bottom == PROOF_HALF;
    printf("compute dispatch writes instances and visible indices: %s\n", dispatch ? "ok" : "FAILED");
    printf("storage buffer read in vertex stage: %s\n", vertex_storage ? "ok" : "FAILED");
    printf("storage buffer read in fragment stage: %s\n", fragment_storage ? "ok" : "FAILED");
    printf(
        "first_instance offsets the per-instance buffer: %s (%u/%u bottom cells)\n",
        first_instance ? "ok" : "FAILED",
        bottom,
        PROOF_HALF
    );
    printf("SDL errors logged: %d\n", error_logs);
    return dispatch && vertex_storage && fragment_storage && first_instance && error_logs == 0;
}

/* Shows the offscreen image, scaled up, until the window is closed. */
static bool run_window(Proof *proof, SDL_Window *native) {
    if (!SDL_ClaimWindowForGPUDevice(proof->device, native)) {
        return fail("window");
    }
    while (ecs_progress()) {
        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(proof->device);
        record(proof, cmd);
        SDL_GPUTexture *swapchain = NULL;
        uint32_t width = 0, height = 0;
        SDL_WaitAndAcquireGPUSwapchainTexture(cmd, native, &swapchain, &width, &height);
        if (swapchain) {
            SDL_BlitGPUTexture(
                cmd,
                &(SDL_GPUBlitInfo){
                    .source = { .texture = proof->target, .w = PROOF_SIZE, .h = PROOF_SIZE },
                    .destination = { .texture = swapchain, .w = width, .h = height },
                    .load_op = SDL_GPU_LOADOP_DONT_CARE,
                    .filter = SDL_GPU_FILTER_NEAREST,
                }
            );
        }
        SDL_SubmitGPUCommandBuffer(cmd);
    }
    SDL_WaitForGPUIdle(proof->device);
    SDL_ReleaseWindowFromGPUDevice(proof->device, native);
    return true;
}

static void release(Proof *proof) {
    if (proof->device) {
        SDL_ReleaseGPUComputePipeline(proof->device, proof->compute);
        SDL_ReleaseGPUGraphicsPipeline(proof->device, proof->pipeline);
        SDL_ReleaseGPUBuffer(proof->device, proof->instances);
        SDL_ReleaseGPUBuffer(proof->device, proof->visible);
        SDL_ReleaseGPUBuffer(proof->device, proof->materials);
        SDL_ReleaseGPUBuffer(proof->device, proof->quad);
        SDL_ReleaseGPUTexture(proof->device, proof->target);
        SDL_DestroyGPUDevice(proof->device);
    }
    gpu_pack_close(&proof->pack);
}

int main(int argc, char **argv) {
    const bool windowed = argc > 1 && strcmp(argv[1], "--window") == 0;
    SDL_GetLogOutputFunction(&default_log, &default_log_data);
    SDL_SetLogOutputFunction(count_errors, NULL);

    Proof proof = { 0 };
    bool ok = gpu_pack_open(&proof.pack, proof_gpushader, sizeof(proof_gpushader));
    if (ok) {
        proof.build = gpu_pack_entry(&proof.pack, "build");
        proof.vertex = gpu_pack_entry(&proof.pack, "vertex_main");
        proof.fragment = gpu_pack_entry(&proof.pack, "fragment_main");
        ok = proof.build && proof.vertex && proof.fragment && check_layouts(&proof);
    }
    printf("pack: %s\n", ok ? proof.pack.header->toolchain : "invalid");

    /* SDL GPU refuses to create a device before the video subsystem is up.
     * Headless runs use the offscreen driver: no window, no display. */
    SDL_Window *native = NULL;
    if (ok && windowed) {
        ecs_init();
        siwindow_import(&(siwindow_props_t){ 0 });
        ecs_entity_t window = ecs_new();
        ecs_set(window, Window, { .slot = "main", .title = "gpu_proof", .width = 512, .height = 512 });
        native = siwindow_native(window);
        ok = native || fail("window");
    } else if (ok) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
        ok = SDL_InitSubSystem(SDL_INIT_VIDEO) || fail("video");
    }
    if (ok) {
        proof.device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, NULL);
        ok = proof.device || fail("device");
    }
    if (ok) {
        printf("device: %s\n", SDL_GetGPUDeviceDriver(proof.device));
        ok = create_resources(&proof) && create_pipelines(&proof);
    }
    if (ok) {
        ok = windowed ? run_window(&proof, native) : run_headless(&proof);
    }
    release(&proof);
    if (windowed) {
        ecs_fini();
    }
    SDL_Quit();
    return ok ? 0 : 1;
}
