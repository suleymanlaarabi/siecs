/*
 * Proof for sigpu v2 (RFC section 16, M3, rebuilt on M4.1): a Slang pack
 * built by addons/gpu/tools/gpushader, a compute dispatch that writes
 * instances and visible indices, and an instanced draw that reads them
 * through storage buffers in the vertex and fragment stages.
 *
 * The device, the shader, the programs, the buffers and the target are
 * sigpu resources created through the ECS, recorded by a render graph: a
 * compute pass, then a render pass whose callback issues the draws with the
 * gpu_cmd_* commands (RFC section 8.4).
 *
 * The second draw starts at first_instance = PROOF_HALF, so the bottom half
 * of the grid only appears when first_instance offsets the per-instance
 * vertex buffer.
 *
 * Headless by default: renders offscreen, reads the pixels back and prints a
 * verdict per point. --window shows the same image in a siwindow window.
 */
#include "proof_pack.generated.h"
#include "sigpu_internal.h"

#include <SDL3/SDL.h>
#include <siwindow.h>
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
#define PROOF_PACK "proof.gpushader"

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
    ecs_entity_t shader, build, draw;
    ecs_entity_t instances, visible, materials, quad, target;
    const gpu_pack_t *pack;
    const gpu_pack_entry_t *build_entry, *vertex, *fragment;
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
    fprintf(stderr, "gpu_proof: %s\n", what);
    return false;
}

static ecs_entity_t buffer(uint32_t size, uint32_t usage) {
    ecs_entity_t entity = ecs_new();
    ecs_set(entity, GpuBuffer, { .size = size, .usage = usage });
    return entity;
}

static void create_resources(Proof *proof) {
    ecs_entity_t device = ecs_new();
    ecs_set(device, GpuDevice, { .debug = true });
    proof->shader = ecs_new();
    ecs_set(proof->shader, GpuShader, { .path = PROOF_PACK });
    proof->build = ecs_new();
    ecs_set(proof->build, GpuProgram, { .shader = proof->shader, .compute = "build" });
    proof->draw = ecs_new();
    ecs_set(proof->draw, GpuProgram, { .shader = proof->shader, .vertex = "vertex_main",
                                       .fragment = "fragment_main" });
    proof->instances = buffer(PROOF_COUNT * sizeof(Instance),
                              GpuBufferGraphicsRead | GpuBufferComputeWrite);
    proof->visible = buffer(PROOF_VISIBLE * sizeof(uint32_t),
                            GpuBufferVertex | GpuBufferComputeWrite);
    proof->materials = buffer(2 * sizeof(Material), GpuBufferGraphicsRead);
    proof->quad = buffer(6 * sizeof(float[2]), GpuBufferVertex);
    if (!proof->target) {
        proof->target = ecs_new();
        ecs_set(proof->target, GpuTexture, {
            .format = GpuFormatRgba8,
            .usage = GpuTextureColorTarget | GpuTextureSampled,
            .width = PROOF_SIZE,
            .height = PROOF_SIZE,
        });
    }
    const Material materials[2] = { { { 1.0f, 0.0f, 0.0f, 1.0f } },
                                    { { 0.0f, 1.0f, 0.0f, 1.0f } } };
    const float quad[6][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 0, 1 }, { 1, 0 }, { 1, 1 } };
    sigpu_buffer_upload(proof->materials, 0, materials, sizeof(materials));
    sigpu_buffer_upload(proof->quad, 0, quad, sizeof(quad));
}

/* One frame builds everything; the failures name the resource and why. */
static bool resources_ready(const Proof *proof) {
    const ecs_entity_t resources[] = { proof->shader,    proof->build,   proof->draw,
                                       proof->instances, proof->visible, proof->materials,
                                       proof->quad,      proof->target };
    bool ready = gpu_session_device() != NULL;
    for (size_t i = 0; i < sizeof(resources) / sizeof(resources[0]); i++) {
        const GpuResourceState *state = ecs_get(resources[i], GpuResourceState);
        if (state->status != GpuReady) {
            fprintf(stderr, "gpu_proof: resource %zu not ready: %s\n", i, state->message);
            ready = false;
        }
    }
    return ready;
}

static bool field_matches(
    const Proof *proof,
    const gpu_pack_resource_t *resource,
    const char *name,
    size_t offset,
    size_t size
) {
    const gpu_pack_field_t *field = gpu_pack_field(proof->pack, resource, name);
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
    const gpu_pack_resource_t *instances = gpu_pack_resource(proof->pack, proof->vertex, "instances");
    const gpu_pack_resource_t *out = gpu_pack_resource(proof->pack, proof->build_entry, "out_instances");
    const gpu_pack_resource_t *materials =
        gpu_pack_resource(proof->pack, proof->fragment, "materials");
    const gpu_pack_resource_t *grid = gpu_pack_resource(proof->pack, proof->build_entry, "grid");
    const gpu_pack_resource_t *view = gpu_pack_resource(proof->pack, proof->vertex, "view");
    if (!instances || !out || !materials || !grid || !view) {
        return fail("the pack does not declare the expected resources");
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
    return ok || fail("C and Slang layouts differ");
}


static void build(gpu_pass_ctx_t *ctx, ecs_entity_t pass, void *user_data) {
    (void)pass;
    (void)user_data;
    const Grid grid = { PROOF_COLUMNS, PROOF_COUNT, PROOF_CELL, PROOF_MARGIN };
    gpu_cmd_uniform(ctx, "grid", &grid, sizeof(grid));
    gpu_cmd_dispatch_cover(ctx, PROOF_COUNT, 1);
}

static void draw(gpu_pass_ctx_t *ctx, ecs_entity_t pass, void *user_data) {
    (void)pass;
    const Proof *proof = user_data;
    /* Uniform blocks are pushed at their reflected size (std140 padding). */
    float view[4] = { (float)PROOF_SIZE, (float)PROOF_SIZE, 0, 0 };
    gpu_cmd_uniform(ctx, "view", view, sizeof(view));
    const GpuVertexLayout layout = {
        .strides = { sizeof(float[2]), sizeof(uint32_t) },
        .instanced = { false, true },
        .attribute_count = 2,
        .attributes = { { .name = "corner", .format = GpuVertexFloat2, .buffer = 0 },
                        { .name = "visible", .format = GpuVertexUint, .buffer = 1 } },
    };
    if (!gpu_cmd_program(ctx, proof->draw, &layout)) {
        return;
    }
    gpu_cmd_vertex_buffer(ctx, 0, proof->quad, 0);
    gpu_cmd_vertex_buffer(ctx, 1, proof->visible, 0);
    gpu_cmd_draw(ctx, 6, PROOF_HALF, 0, 0);
    gpu_cmd_draw(ctx, 6, PROOF_HALF, 0, PROOF_HALF);
}

/* The compute pass and the draw pass, in graph; target is the color the
 * draw writes. */
static void create_passes(Proof *proof, ecs_entity_t graph) {
    ecs_entity_t compute = sigpu_pass(graph, &(GpuPassDesc){
        .name = "proof_build", .kind = GpuPassCompute, .order = GPU_ORDER_SCENE,
        .program = proof->build, .execute = build });
    sigpu_pass_storage_write(compute, "out_instances", proof->instances);
    sigpu_pass_storage_write(compute, "out_visible", proof->visible);
    ecs_entity_t render = sigpu_pass(graph, &(GpuPassDesc){
        .name = "proof_draw", .kind = GpuPassRender, .order = GPU_ORDER_SCENE + 1,
        .execute = draw, .user_data = proof });
    sigpu_pass_color(render, proof->target, GpuLoadClear, GpuStoreStore,
                     (const float[4]){ 0, 0, 0, 1 }, 0);
    sigpu_pass_storage_read(render, "instances", proof->instances);
    sigpu_pass_storage_read(render, "materials", proof->materials);
    sigpu_pass_access(render, &(GpuAccess){ .resource = proof->visible, .usage = GpuUseVertex });
    sigpu_pass_access(render, &(GpuAccess){ .resource = proof->quad, .usage = GpuUseVertex });
}

static bool pixel_is(const uint8_t *pixels, uint32_t x, uint32_t y, const uint8_t rgba[4]) {
    return memcmp(pixels + (y * PROOF_SIZE + x) * 4, rgba, 4) == 0;
}

static bool read_target(const Proof *proof, uint8_t *pixels) {
    SDL_GPUDevice *gpu = gpu_session_device();
    const uint32_t size = PROOF_SIZE * PROOF_SIZE * 4;
    SDL_GPUTransferBuffer *download = SDL_CreateGPUTransferBuffer(
        gpu,
        &(SDL_GPUTransferBufferCreateInfo){ .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
                                            .size = size }
    );
    SDL_GPUCommandBuffer *cmd = download ? SDL_AcquireGPUCommandBuffer(gpu) : NULL;
    if (!cmd) {
        return fail("download buffer");
    }
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_DownloadFromGPUTexture(
        copy,
        &(SDL_GPUTextureRegion){ .texture = gpu_resource_texture(proof->target, NULL),
                                 .w = PROOF_SIZE, .h = PROOF_SIZE, .d = 1 },
        &(SDL_GPUTextureTransferInfo){ .transfer_buffer = download }
    );
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    const bool done = fence && SDL_WaitForGPUFences(gpu, true, &fence, 1);
    SDL_ReleaseGPUFence(gpu, fence);
    const void *mapped = done ? SDL_MapGPUTransferBuffer(gpu, download, false) : NULL;
    if (mapped) {
        memcpy(pixels, mapped, size);
        SDL_UnmapGPUTransferBuffer(gpu, download);
    }
    SDL_ReleaseGPUTransferBuffer(gpu, download);
    return mapped != NULL || fail("readback");
}

/* Reads the target back and prints one verdict per point. */
static bool verdicts(const Proof *proof) {
    static uint8_t pixels[PROOF_SIZE * PROOF_SIZE * 4];
    if (!read_target(proof, pixels)) {
        return false;
    }
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

int main(int argc, char **argv) {
    const bool windowed = argc > 1 && strcmp(argv[1], "--window") == 0;
    SDL_GetLogOutputFunction(&default_log, &default_log_data);
    SDL_SetLogOutputFunction(count_errors, NULL);

    ecs_init();
    /* siwindow first: without it, sigpu starts the video subsystem on the
     * offscreen driver. */
    if (windowed) {
        ECS_MODULE_IMPORT(siwindow, { 0 });
        ecs_set(ecs_new(), Window, { .slot = "main", .title = "gpu_proof", .width = 512,
                                     .height = 512 });
    }
    ECS_MODULE_IMPORT(sigpu, { 0 });
    sigpu_asset_register(PROOF_PACK, proof_gpushader, sizeof(proof_gpushader));

    Proof proof = { 0 };
    ecs_entity_t surface = 0;
    if (windowed) {
        /* The draw scales PROOF_SIZE pixels to the surface's color target. */
        surface = ecs_new();
        ecs_set(surface, GpuSurface, { .window_slot = "main" });
        for (int i = 0; i < 4 && !ecs_try_get(surface, GpuSurfaceTargets); i++) {
            ecs_progress();
        }
    }
    create_resources(&proof);
    const GpuSurfaceTargets *targets = surface ? ecs_try_get(surface, GpuSurfaceTargets) : NULL;
    ecs_entity_t graph = 0;
    if (targets) {
        proof.target = targets->color;
        graph = targets->graph;
    } else {
        graph = sigpu_graph(0, "proof", 0);
        sigpu_graph_export(graph, proof.target);
    }
    create_passes(&proof, graph);
    ecs_progress();
    ecs_progress();
    bool ok = (!windowed || targets || fail("window")) && resources_ready(&proof);
    if (ok) {
        proof.pack = gpu_resource_program_pack(proof.build, NULL, NULL, &proof.build_entry);
        gpu_resource_program_pack(proof.draw, &proof.vertex, &proof.fragment, NULL);
        printf("pack: %s\n", proof.pack->header->toolchain);
        printf("device: %s\n", ecs_get_resource_read(GpuSession)->driver);
        ok = check_layouts(&proof);
        const GpuGraphState *state = ecs_get(graph, GpuGraphState);
        printf("graph: %u passes, %s\n", state->passes,
               state->status == GpuReady ? "ready" : state->message);
        ok = ok && state->status == GpuReady;
    }
    if (ok && windowed) {
        while (ecs_progress()) {
        }
    } else if (ok) {
        ok = verdicts(&proof);
    }
    ecs_fini();
    SDL_Quit();
    return ok ? 0 : 1;
}
