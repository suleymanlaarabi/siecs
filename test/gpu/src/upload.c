#include "util.h"
#include <string.h>

void upload_buffer_upload_lands(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t buffer = gpu_test_buffer(16);
    const float data[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    test_true(sigpu_buffer_upload(buffer, 0, data, sizeof(data)));
    test_true(sigpu_buffer_upload(buffer, 8, &data[0], 4));
    gpu_test_frames(1);
    float memory[4];
    test_true(gpu_test_read_buffer(buffer, 0, memory, sizeof(memory)));
    test_flt(1.0f, memory[0]);
    test_flt(1.0f, memory[2]);
    test_flt(4.0f, memory[3]);
    test_uint(2, gpu_test_stats()->uploads);
    test_uint(20, gpu_test_stats()->upload_bytes);
    test_uint(1, gpu_test_stats()->submissions);

    /* No new data: no command buffer. */
    gpu_test_frames(1);
    test_uint(0, gpu_test_stats()->submissions);
    test_uint(1, gpu_test_session()->submissions);
    ecs_fini();
}

void upload_upload_waits_for_ready(void) {
    gpu_test_start(0);
    ecs_entity_t buffer = gpu_test_buffer(16);
    const uint32_t data[4] = { 5, 6, 7, 8 };
    test_true(sigpu_buffer_upload(buffer, 0, data, sizeof(data)));
    gpu_test_frames(2);
    gpu_test_device(0);
    gpu_test_frames(1);
    uint32_t memory[4];
    test_true(gpu_test_read_buffer(buffer, 0, memory, sizeof(memory)));
    test_assert(memcmp(memory, data, sizeof(data)) == 0);
    ecs_fini();
}

void upload_upload_out_of_range_rejected(void) {
    gpu_test_start(0);
    ecs_entity_t buffer = gpu_test_buffer(16);
    ecs_entity_t texture = gpu_test_texture(4, 4);
    const uint32_t data[4] = { 0 };
    test_false(sigpu_buffer_upload(buffer, 8, data, sizeof(data)));
    test_false(sigpu_buffer_upload(buffer, 0, NULL, 4));
    test_false(sigpu_buffer_upload(buffer, 0, data, 0));
    test_false(sigpu_buffer_upload(texture, 0, data, 4));
    test_false(sigpu_texture_upload(buffer, &(GpuTextureRegion){ .width = 1, .height = 1,
                                                                .depth = 1 }, data, 4, 0));
    test_true(sigpu_buffer_upload(buffer, 12, data, 4));
    ecs_fini();
}

void upload_texture_upload_pitch(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t texture = gpu_test_texture(4, 2);
    /* Two rows of 4 RGBA8 texels, 32 bytes apart; the last row stops at
     * the region width. */
    uint8_t data[48];
    for (uint32_t i = 0; i < sizeof(data); i++) {
        data[i] = (uint8_t)i;
    }
    const GpuTextureRegion region = { .width = 4, .height = 2, .depth = 1 };
    test_false(sigpu_texture_upload(texture, &region, data, sizeof(data), 30));
    test_false(sigpu_texture_upload(texture, &region, data, 47, 32));
    test_true(sigpu_texture_upload(texture, &region, data, sizeof(data), 32));
    gpu_test_frames(1);

    uint8_t memory[32];
    test_true(gpu_test_read_texture(texture, 4, 2, memory));
    test_assert(memcmp(memory, data, 16) == 0);
    test_assert(memcmp(memory + 16, data + 32, 16) == 0);
    ecs_fini();
}

void upload_upload_dropped_with_buffer(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t buffer = gpu_test_buffer(16);
    const uint32_t data[4] = { 0 };
    test_true(sigpu_buffer_upload(buffer, 0, data, sizeof(data)));
    ecs_kill(buffer);
    gpu_test_frames(1);
    test_uint(0, gpu_test_stats()->uploads);
    test_uint(0, gpu_test_session()->submissions);
    ecs_fini();
}
