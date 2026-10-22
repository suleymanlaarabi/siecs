#include "util.h"
#include <string.h>

void lifecycle_created_out_of_order(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t shader = ecs_new();
    ecs_entity_t program = gpu_test_compute(shader, "fill");
    ecs_set(shader, GpuShader, { .path = TEST_PACK });
    gpu_test_frames(1);
    test_true(sigpu_ready(shader));
    test_true(sigpu_ready(program));
    test_uint(1, gpu_test_stats()->created);

    const gpu_pack_entry_t *compute = NULL;
    const gpu_pack_t *pack = gpu_resource_program_pack(program, NULL, NULL, &compute);
    test_assert(pack != NULL);
    test_str("fill", compute->name);
    test_uint(32, compute->threads[0]);
    ecs_fini();
}

void lifecycle_sets_coalesce(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t buffer = ecs_new();
    ecs_set(buffer, GpuBuffer, { .size = 16, .usage = GpuBufferVertex });
    ecs_set(buffer, GpuBuffer, { .size = 32, .usage = GpuBufferVertex });
    ecs_set(buffer, GpuBuffer, { .size = 64, .usage = GpuBufferVertex });
    gpu_test_frames(1);
    test_uint(1, gpu_test_stats()->created);
    test_uint(1, gpu_test_stats()->processed);
    const GpuResourceState *state = gpu_test_state(buffer);
    test_uint(3, state->requested);
    test_uint(3, state->applied);
    test_uint(64, gpu_resource_record(buffer, GPU_RESOURCE_BUFFER)->as.buffer.size);
    ecs_fini();
}

void lifecycle_waiting_is_not_retried(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t shader = gpu_test_shader("missing.gpushader");
    ecs_entity_t program = gpu_test_compute(shader, "fill");
    gpu_test_frames(1);
    test_uint(GpuFailed, gpu_test_state(shader)->status);
    test_uint(GpuErrorPack, gpu_test_state(shader)->error);
    test_uint(GpuWaiting, gpu_test_state(program)->status);
    test_uint(shader, gpu_test_state(program)->dependency);
    for (uint32_t i = 0; i < 3; i++) {
        gpu_test_frames(1);
        test_uint(0, gpu_test_stats()->processed);
    }
    test_uint(0, gpu_test_stats()->objects);
    ecs_fini();
}

void lifecycle_failure_then_fix(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t shader = gpu_test_shader("late.gpushader");
    ecs_entity_t program = gpu_test_compute(shader, "fill");
    gpu_test_frames(1);
    test_uint(GpuFailed, gpu_test_state(shader)->status);
    test_assert(strstr(gpu_test_state(shader)->message, "late.gpushader") != NULL);

    size_t size = 0;
    const unsigned char *pack = gpu_test_pack(&size);
    test_true(sigpu_asset_register("late.gpushader", pack, size));
    gpu_test_frames(1);
    test_true(sigpu_ready(shader));
    test_true(sigpu_ready(program));
    test_uint(GpuErrorNone, gpu_test_state(shader)->error);
    test_str("", gpu_test_state(shader)->message);
    ecs_fini();
}

void lifecycle_invalid_then_corrected(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t buffer = ecs_new();
    ecs_set(buffer, GpuBuffer, { .size = 6, .usage = GpuBufferVertex });
    gpu_test_frames(1);
    test_uint(GpuFailed, gpu_test_state(buffer)->status);
    test_uint(GpuErrorInvalid, gpu_test_state(buffer)->error);
    test_assert(gpu_test_state(buffer)->message[0] != '\0');
    test_uint(0, gpu_test_stats()->created);

    ecs_set(buffer, GpuBuffer, { .size = 16, .usage = GpuBufferVertex });
    gpu_test_frames(1);
    test_true(sigpu_ready(buffer));
    test_uint(2, gpu_test_state(buffer)->applied);
    ecs_fini();
}

void lifecycle_removed_before_ready(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    gpu_test_frames(1);
    ecs_entity_t buffer = gpu_test_buffer(16);
    ecs_kill(buffer);
    gpu_test_frames(1);
    test_uint(0, gpu_test_stats()->created);
    test_uint(0, gpu_test_stats()->processed);
    ecs_fini();
}

void lifecycle_removed_after_ready(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t buffer = gpu_test_buffer(16);
    gpu_test_frames(1);
    test_true(sigpu_ready(buffer));

    ecs_remove(buffer, GpuBuffer);
    test_uint(GpuRetiring, gpu_test_state(buffer)->status);
    test_false(sigpu_ready(buffer));
    gpu_test_frames(1);
    test_uint(0, gpu_test_stats()->objects);
    test_uint(1, gpu_test_stats()->released);
    test_uint(GpuRetiring, gpu_test_state(buffer)->status);
    ecs_fini();
}

void lifecycle_resize_replaces_object(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t buffer = gpu_test_buffer(16);
    gpu_test_frames(1);

    ecs_set(buffer, GpuBuffer, { .size = 32, .usage = GpuBufferVertex });
    gpu_test_frames(1);
    test_true(sigpu_ready(buffer));
    test_uint(32, gpu_resource_record(buffer, GPU_RESOURCE_BUFFER)->as.buffer.size);
    test_uint(2, gpu_test_state(buffer)->allocation);
    test_uint(1, gpu_test_stats()->created);
    test_uint(1, gpu_test_stats()->released);
    test_uint(1, gpu_test_stats()->objects);
    ecs_fini();
}

static const gpu_texture_info_t *texture_info(ecs_entity_t texture) {
    const gpu_record_t *record = gpu_resource_record(texture, GPU_RESOURCE_TEXTURE);
    test_assert(record != NULL);
    return &record->as.texture.info;
}

void lifecycle_size_from_follows_source(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t source = gpu_test_texture(100, 60);
    ecs_entity_t half = ecs_new();
    ecs_set(half, GpuTexture, { .format = GpuFormatRgba16f, .usage = GpuTextureColorTarget,
                                .size_from = source, .scale = 0.5f });
    gpu_test_frames(1);
    test_uint(50, texture_info(half)->width);
    test_uint(30, texture_info(half)->height);

    ecs_set(source, GpuTexture, { .format = GpuFormatRgba8, .usage = GpuTextureColorTarget,
                                  .width = 201, .height = 100 });
    gpu_test_frames(1);
    test_uint(101, texture_info(half)->width);
    test_uint(50, texture_info(half)->height);
    test_uint(2, gpu_test_state(half)->allocation);

    /* A new source allocation of the same size keeps the dependent. */
    ecs_set(source, GpuTexture, { .format = GpuFormatRgba8, .usage = GpuTextureSampled |
                                  GpuTextureColorTarget, .width = 201, .height = 100 });
    gpu_test_frames(1);
    test_uint(3, gpu_test_state(source)->allocation);
    test_uint(2, gpu_test_state(half)->allocation);
    test_true(sigpu_ready(half));
    ecs_fini();
}

void lifecycle_size_from_cycle_fails(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t a = ecs_new();
    ecs_entity_t b = ecs_new();
    ecs_set(a, GpuTexture, { .format = GpuFormatRgba8, .usage = GpuTextureSampled, .size_from = b });
    ecs_set(b, GpuTexture, { .format = GpuFormatRgba8, .usage = GpuTextureSampled, .size_from = a });
    gpu_test_frames(1);
    test_uint(GpuFailed, gpu_test_state(a)->status);
    test_uint(GpuErrorCycle, gpu_test_state(a)->error);
    test_uint(GpuFailed, gpu_test_state(b)->status);
    test_uint(GpuErrorCycle, gpu_test_state(b)->error);
    ecs_fini();
}

void lifecycle_dependency_killed_fails(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t shader = gpu_test_shader(TEST_PACK);
    ecs_entity_t program = gpu_test_compute(shader, "fill");
    gpu_test_frames(1);
    test_true(sigpu_ready(program));

    ecs_kill(shader);
    gpu_test_frames(1);
    test_uint(GpuFailed, gpu_test_state(program)->status);
    test_uint(GpuErrorDependency, gpu_test_state(program)->error);
    test_uint(0, gpu_test_stats()->objects);
    ecs_fini();
}

void lifecycle_entry_missing_fails(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t shader = gpu_test_shader(TEST_PACK);
    ecs_entity_t missing = gpu_test_compute(shader, "nope");
    ecs_entity_t wrong_stage = ecs_new();
    ecs_set(wrong_stage, GpuProgram, { .shader = shader, .vertex = "fill",
                                       .fragment = "fragment_main" });
    ecs_entity_t graphics = ecs_new();
    ecs_set(graphics, GpuProgram, { .shader = shader, .vertex = "vertex_main",
                                    .fragment = "fragment_main" });
    gpu_test_frames(1);
    test_uint(GpuErrorEntry, gpu_test_state(missing)->error);
    test_uint(GpuErrorEntry, gpu_test_state(wrong_stage)->error);
    test_true(sigpu_ready(graphics));
    test_uint(2, gpu_test_stats()->objects);
    ecs_fini();
}

void lifecycle_budget_defers(void) {
    gpu_test_start(2);
    gpu_test_device(0);
    ecs_entity_t buffers[5];
    for (uint32_t i = 0; i < 5; i++) {
        buffers[i] = gpu_test_buffer(16);
    }
    const uint32_t expected[3] = { 2, 4, 5 };
    for (uint32_t frame = 0; frame < 3; frame++) {
        gpu_test_frames(1);
        uint32_t ready = 0;
        for (uint32_t i = 0; i < 5; i++) {
            ready += sigpu_ready(buffers[i]);
        }
        test_uint(expected[frame], ready);
        test_uint(5 - expected[frame], gpu_test_stats()->deferred);
    }
    ecs_fini();
}

void lifecycle_no_device_stays_pending(void) {
    gpu_test_start(0);
    ecs_entity_t buffer = gpu_test_buffer(16);
    gpu_test_frames(2);
    test_uint(GpuPending, gpu_test_state(buffer)->status);
    test_false(gpu_test_session()->ready);
    gpu_test_device(0);
    gpu_test_frames(1);
    test_true(sigpu_ready(buffer));
    ecs_fini();
}

void lifecycle_second_device_waits(void) {
    gpu_test_start(0);
    ecs_entity_t first = gpu_test_device(0);
    ecs_entity_t second = gpu_test_device(0);
    ecs_entity_t buffer = gpu_test_buffer(16);
    gpu_test_frames(1);
    test_uint(GpuReady, gpu_test_state(first)->status);
    test_uint(GpuWaiting, gpu_test_state(second)->status);
    test_uint(first, gpu_test_state(second)->dependency);
    test_true(sigpu_ready(buffer));

    ecs_remove(first, GpuDevice);
    gpu_test_frames(1);
    test_uint(GpuRetiring, gpu_test_state(first)->status);
    test_uint(GpuReady, gpu_test_state(second)->status);
    test_uint(second, gpu_test_session()->device);
    test_true(sigpu_ready(buffer));
    test_uint(2, gpu_test_state(buffer)->allocation);
    ecs_fini();
}

void lifecycle_state_before_device_opens(void) {
    gpu_test_start(0);
    ecs_entity_t device = ecs_new();
    ecs_add(device, GpuResourceState);
    ecs_set(device, GpuDevice, { 0 });
    ecs_entity_t buffer = gpu_test_buffer(16);
    gpu_test_frames(1);
    test_uint(GpuReady, gpu_test_state(device)->status);
    test_uint(device, gpu_test_session()->device);
    test_true(sigpu_ready(buffer));
    ecs_fini();
}

void lifecycle_device_recreated(void) {
    gpu_test_start(0);
    ecs_entity_t device = gpu_test_device(0);
    ecs_entity_t buffer = gpu_test_buffer(16);
    /* An IsA base is Abstract, hidden from default queries. */
    ecs_entity_t base = gpu_test_buffer(16);
    ecs_entity_t instance = ecs_new();
    ecs_is_a(instance, base);
    gpu_test_frames(1);
    test_true(sigpu_ready(buffer));
    test_true(sigpu_ready(base));
    test_true(sigpu_ready(instance));

    ecs_remove(device, GpuDevice);
    gpu_test_frames(1);
    test_uint(GpuPending, gpu_test_state(buffer)->status);
    test_uint(0, gpu_test_state(buffer)->record);
    test_false(gpu_test_session()->ready);
    test_assert(gpu_session_device() == NULL);

    test_uint(GpuPending, gpu_test_state(base)->status);
    test_uint(GpuPending, gpu_test_state(instance)->status);

    ecs_set(device, GpuDevice, { 0 });
    gpu_test_frames(1);
    test_true(sigpu_ready(buffer));
    test_true(sigpu_ready(base));
    test_true(sigpu_ready(instance));
    test_uint(2, gpu_test_state(buffer)->allocation);
    test_uint(1, gpu_test_state(buffer)->applied);
    test_uint(3, gpu_test_stats()->objects);
    ecs_fini();
}
