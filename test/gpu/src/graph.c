#include "util.h"

#define RED 0xff0000ffu
#define GREEN 0xff00ff00u
#define BLUE 0xffff0000u
#define BLACK 0xff000000u

typedef struct {
    ecs_entity_t program;
    float color[4];
} fill_t;

static void fill(gpu_pass_ctx_t *ctx, ecs_entity_t pass, void *user_data) {
    (void)pass;
    fill_t *data = user_data;
    gpu_cmd_uniform(ctx, "fill_color", data->color, sizeof(data->color));
    test_true(gpu_cmd_program(ctx, data->program, NULL));
    gpu_cmd_draw(ctx, 3, 1, 0, 0);
}

/* Programs of the custom pass tests. */
static ecs_entity_t solid_program(void) {
    return gpu_test_graphics(gpu_test_shader(TEST_PACK), "fullscreen", "solid");
}

static ecs_entity_t fill_pass(ecs_entity_t graph, int32_t order, fill_t *data, ecs_entity_t target,
                              uint8_t load, const float *viewport) {
    ecs_entity_t pass = sigpu_pass(graph, &(GpuPassDesc){ .kind = GpuPassRender, .order = order,
                                                          .execute = fill, .user_data = data });
    sigpu_pass_color(pass, target, load, GpuStoreStore, (const float[4]){ 1, 0, 0, 1 }, 0);
    if (viewport) {
        GpuPass desc = *ecs_get(pass, GpuPass);
        memcpy(desc.viewport, viewport, sizeof(desc.viewport));
        ecs_set_cid(pass, ecs_id(GpuPass), &desc);
    }
    return pass;
}

static const GpuGraphState *graph_state(ecs_entity_t graph) {
    return ecs_get(graph, GpuGraphState);
}

void graph_order_deduced(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t target = gpu_test_target(8, 8);
    fill_t green = { solid_program(), { 0, 1, 0, 1 } };
    ecs_entity_t graph = sigpu_graph(0, NULL, 0);
    /* Created first but ordered last: draws the left half over the clear. */
    fill_pass(graph, 10, &green, target, GpuLoadLoad, (const float[4]){ 0, 0, 0.5f, 1 });
    fill_t red = { green.program, { 1, 0, 0, 1 } };
    fill_pass(graph, 0, &red, target, GpuLoadClear, NULL);
    sigpu_graph_export(graph, target);
    gpu_test_frames(2);
    test_uint(GpuReady, graph_state(graph)->status);
    test_uint(2, graph_state(graph)->passes);
    test_uint(GREEN, gpu_test_pixel(target, 1, 4));
    test_uint(RED, gpu_test_pixel(target, 6, 4));
    ecs_fini();
}

void graph_clear_load_store(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t target = gpu_test_target(4, 4);
    ecs_entity_t graph = sigpu_graph(0, NULL, 0);
    ecs_entity_t clear = sigpu_pass(graph, &(GpuPassDesc){ .kind = GpuPassRender });
    sigpu_pass_color(clear, target, GpuLoadClear, GpuStoreStore, (const float[4]){ 0, 0, 1, 1 }, 0);
    ecs_entity_t keep = sigpu_pass(graph, &(GpuPassDesc){ .kind = GpuPassRender, .order = 1 });
    sigpu_pass_color(keep, target, GpuLoadLoad, GpuStoreStore, NULL, 0);
    sigpu_graph_export(graph, target);
    gpu_test_frames(1);
    test_uint(GpuReady, graph_state(graph)->status);
    test_uint(BLUE, gpu_test_pixel(target, 2, 2));
    /* Discarding the content then loading it is rejected. */
    GpuPassIO io = *ecs_get(clear, GpuPassIO);
    io.items[0].store = GpuStoreDontCare;
    ecs_set_cid(clear, ecs_id(GpuPassIO), &io);
    gpu_test_frames(1);
    test_uint(GpuFailed, graph_state(graph)->status);
    test_uint(keep, graph_state(graph)->pass);
    test_assert(strstr(graph_state(graph)->message, "discarded") != NULL);
    ecs_fini();
}

static void draw_fullscreen(gpu_pass_ctx_t *ctx, ecs_entity_t pass, void *user_data) {
    (void)pass;
    (void)user_data;
    gpu_cmd_draw(ctx, 3, 1, 0, 0);
}

void graph_uninitialized_read_rejected(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t written = gpu_test_target(4, 4);
    ecs_entity_t output = gpu_test_target(4, 4);
    ecs_entity_t shader = gpu_test_shader(TEST_PACK);
    ecs_entity_t copy = gpu_test_graphics(shader, "fullscreen", "copy");
    ecs_entity_t graph = sigpu_graph(0, NULL, 0);
    ecs_entity_t reader = sigpu_pass(graph, &(GpuPassDesc){ .kind = GpuPassRender,
                                                            .program = copy,
                                                            .execute = draw_fullscreen });
    sigpu_pass_sample(reader, "source", written, gpu_test_sampler());
    sigpu_pass_color(reader, output, GpuLoadDontCare, GpuStoreStore, NULL, 0);
    ecs_entity_t writer = sigpu_pass(graph, &(GpuPassDesc){ .kind = GpuPassRender, .order = 5 });
    sigpu_pass_color(writer, written, GpuLoadClear, GpuStoreStore, NULL, 0);
    sigpu_graph_export(graph, output);
    gpu_test_frames(2);
    test_uint(GpuFailed, graph_state(graph)->status);
    test_uint(GpuErrorInterface, graph_state(graph)->error);
    test_uint(reader, graph_state(graph)->pass);
    test_assert(strstr(graph_state(graph)->message, "before any pass writes it") != NULL);
    /* Writing first fixes it, and the copy reads what was written. */
    GpuPass desc = *ecs_get(writer, GpuPass);
    desc.order = -5;
    ecs_set_cid(writer, ecs_id(GpuPass), &desc);
    GpuPassIO io = *ecs_get(writer, GpuPassIO);
    io.items[0].clear[0] = 0;
    io.items[0].clear[1] = 1;
    io.items[0].clear[3] = 1;
    ecs_set_cid(writer, ecs_id(GpuPassIO), &io);
    gpu_test_frames(1);
    test_uint(GpuReady, graph_state(graph)->status);
    test_uint(GREEN, gpu_test_pixel(output, 1, 1));
    ecs_fini();
}

void graph_sampled_attachment_rejected(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t target = gpu_test_target(4, 4);
    ecs_entity_t graph = sigpu_graph(0, NULL, 0);
    ecs_entity_t pass = sigpu_pass(graph, &(GpuPassDesc){ .kind = GpuPassRender });
    sigpu_pass_color(pass, target, GpuLoadClear, GpuStoreStore, NULL, 0);
    sigpu_pass_sample(pass, "source", target, 0);
    sigpu_graph_export(graph, target);
    gpu_test_frames(1);
    test_uint(GpuFailed, graph_state(graph)->status);
    test_assert(strstr(graph_state(graph)->message, "sampled and attached") != NULL);
    ecs_fini();
}

void graph_unused_pass_culled(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t used = gpu_test_target(4, 4);
    ecs_entity_t unused = gpu_test_target(4, 4);
    ecs_entity_t graph = sigpu_graph(0, NULL, 0);
    ecs_entity_t a = sigpu_pass(graph, &(GpuPassDesc){ .kind = GpuPassRender });
    sigpu_pass_color(a, used, GpuLoadClear, GpuStoreStore, NULL, 0);
    ecs_entity_t b = sigpu_pass(graph, &(GpuPassDesc){ .kind = GpuPassRender });
    sigpu_pass_color(b, unused, GpuLoadClear, GpuStoreStore, NULL, 0);
    sigpu_graph_export(graph, used);
    gpu_test_frames(1);
    test_uint(1, graph_state(graph)->passes);
    test_uint(1, graph_state(graph)->culled);
    test_uint(1, gpu_test_stats()->passes);
    /* An exported or side-effect pass is kept. */
    sigpu_graph_export(graph, unused);
    gpu_test_frames(1);
    test_uint(2, graph_state(graph)->passes);
    test_uint(0, graph_state(graph)->culled);
    /* A clear fully overwritten before its only reader is dead. */
    ecs_entity_t overwrite = sigpu_pass(graph, &(GpuPassDesc){ .kind = GpuPassRender, .order = 1 });
    sigpu_pass_color(overwrite, used, GpuLoadDontCare, GpuStoreStore, NULL, 0);
    gpu_test_frames(1);
    test_uint(2, graph_state(graph)->passes);
    test_uint(1, graph_state(graph)->culled);
    ecs_fini();
}

void graph_compiles_once(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t target = gpu_test_target(4, 4);
    ecs_entity_t graph = sigpu_graph(0, NULL, 0);
    ecs_entity_t pass = sigpu_pass(graph, &(GpuPassDesc){ .kind = GpuPassRender });
    sigpu_pass_color(pass, target, GpuLoadClear, GpuStoreStore, NULL, 0);
    sigpu_graph_export(graph, target);
    gpu_test_frames(2);
    const uint32_t revision = graph_state(graph)->revision;
    for (uint32_t i = 0; i < 5; i++) {
        gpu_test_frames(1);
        test_uint(0, gpu_test_stats()->graph_compiles);
        test_uint(1, gpu_test_stats()->passes);
    }
    test_uint(revision, graph_state(graph)->revision);
    ecs_fini();
}

void graph_resize_without_compile(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t base = gpu_test_target(8, 8);
    ecs_entity_t target = ecs_new();
    ecs_set(target, GpuTexture, { .format = GpuFormatRgba8,
                                  .usage = GpuTextureColorTarget | GpuTextureSampled,
                                  .size_from = base, .scale = 0.5f });
    ecs_entity_t graph = sigpu_graph(0, NULL, 0);
    ecs_entity_t pass = sigpu_pass(graph, &(GpuPassDesc){ .kind = GpuPassRender });
    sigpu_pass_color(pass, target, GpuLoadClear, GpuStoreStore, (const float[4]){ 0, 1, 0, 1 }, 0);
    sigpu_graph_export(graph, target);
    gpu_test_frames(3);
    const uint32_t revision = graph_state(graph)->revision;
    const uint32_t allocation = gpu_test_state(target)->allocation;
    /* The base is not in the graph: resizing it reallocates the target
     * without touching the plan. */
    ecs_set(base, GpuTexture, { .format = GpuFormatRgba8,
                                .usage = GpuTextureSampled | GpuTextureColorTarget,
                                .width = 16, .height = 16 });
    gpu_test_frames(3);
    uint32_t width = 0, height = 0;
    test_true(sigpu_extent(target, &width, &height));
    test_uint(8, width);
    test_uint(allocation + 1, gpu_test_state(target)->allocation);
    test_uint(GREEN, gpu_test_pixel(target, 7, 7));
    test_uint(revision, graph_state(graph)->revision);
    ecs_fini();
}

void graph_waits_for_resource(void) {
    gpu_test_start(0);
    ecs_entity_t target = gpu_test_target(4, 4);
    ecs_entity_t graph = sigpu_graph(0, NULL, 0);
    ecs_entity_t pass = sigpu_pass(graph, &(GpuPassDesc){ .kind = GpuPassRender });
    sigpu_pass_color(pass, target, GpuLoadClear, GpuStoreStore, NULL, 0);
    sigpu_graph_export(graph, target);
    gpu_test_frames(1);
    test_uint(GpuPending, graph_state(graph)->status);
    gpu_test_device(0);
    gpu_test_frames(2);
    test_uint(GpuReady, graph_state(graph)->status);
    ecs_fini();
}

typedef struct {
    ecs_entity_t program;
    uint32_t count;
    float value;
} compute_t;

static void compute(gpu_pass_ctx_t *ctx, ecs_entity_t pass, void *user_data) {
    (void)pass;
    compute_t *data = user_data;
    const struct {
        uint32_t count;
        float value;
    } params = { data->count, data->value };
    gpu_cmd_uniform(ctx, "params", &params, sizeof(params));
    test_true(gpu_cmd_program(ctx, data->program, NULL));
    gpu_cmd_dispatch_cover(ctx, data->count, 1);
}

void graph_compute_pass(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t values = ecs_new();
    ecs_set(values, GpuBuffer, { .size = 64 * 4, .usage = GpuBufferComputeWrite });
    compute_t data = { gpu_test_compute(gpu_test_shader(TEST_PACK), "fill"), 64, 2.5f };
    ecs_entity_t graph = sigpu_graph(0, NULL, 0);
    ecs_entity_t pass = sigpu_pass(graph, &(GpuPassDesc){ .kind = GpuPassCompute,
                                                          .program = data.program,
                                                          .execute = compute,
                                                          .user_data = &data });
    sigpu_pass_storage_write(pass, "values", values);
    sigpu_graph_export(graph, values);
    gpu_test_frames(2);
    test_uint(GpuReady, ecs_get(graph, GpuGraphState)->status);
    float out[64];
    test_true(gpu_test_read_buffer(values, 0, out, sizeof(out)));
    test_flt(2.5f, out[0]);
    test_flt(2.5f, out[63]);
    ecs_fini();
}

typedef struct {
    ecs_entity_t from, to;
} copy_t;

static void copy_buffers(gpu_pass_ctx_t *ctx, ecs_entity_t pass, void *user_data) {
    (void)pass;
    copy_t *data = user_data;
    gpu_cmd_copy_buffer(ctx, data->from, 4, data->to, 0, 8);
}

void graph_copy_pass(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    copy_t data = { gpu_test_buffer(16), gpu_test_buffer(8) };
    const uint32_t values[4] = { 1, 2, 3, 4 };
    sigpu_buffer_upload(data.from, 0, values, sizeof(values));
    ecs_entity_t graph = sigpu_graph(0, NULL, 0);
    ecs_entity_t pass = sigpu_pass(graph, &(GpuPassDesc){ .kind = GpuPassCopy,
                                                          .execute = copy_buffers,
                                                          .user_data = &data });
    sigpu_pass_access(pass, &(GpuAccess){ .resource = data.from, .usage = GpuUseCopySrc });
    sigpu_pass_access(pass, &(GpuAccess){ .resource = data.to, .usage = GpuUseCopyDst });
    sigpu_graph_export(graph, data.to);
    gpu_test_frames(2);
    uint32_t out[2];
    test_true(gpu_test_read_buffer(data.to, 0, out, sizeof(out)));
    test_uint(2, out[0]);
    test_uint(3, out[1]);
    ecs_fini();
}

static int destroyed;

static void count_destroy(void *data) {
    (void)data;
    destroyed++;
}

static void nothing(gpu_pass_ctx_t *ctx, ecs_entity_t pass, void *user_data) {
    (void)ctx;
    (void)pass;
    (void)user_data;
}

void graph_callback_destroyed_once(void) {
    destroyed = 0;
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t graph = sigpu_graph(0, NULL, 0);
    ecs_entity_t a = sigpu_pass(graph, &(GpuPassDesc){ .execute = nothing,
                                                       .user_data_dtor = count_destroy });
    sigpu_pass(graph, &(GpuPassDesc){ .execute = nothing, .user_data_dtor = count_destroy });
    /* Replacing destroys the previous one. */
    sigpu_pass_execute(a, nothing, NULL, count_destroy);
    test_int(1, destroyed);
    ecs_kill(a);
    test_int(2, destroyed);
    gpu_test_frames(1);
    ecs_fini();
    test_int(3, destroyed);
}
