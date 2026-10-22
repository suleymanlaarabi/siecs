#include "util.h"

void frame_upload_every_frame(void) {
    gpu_test_start(0);
    gpu_test_device(2);
    ecs_entity_t buffer = gpu_test_buffer(16);
    gpu_test_frames(1);
    for (uint32_t frame = 0; frame < 6; frame++) {
        const uint32_t data[4] = { frame, frame + 1, frame + 2, frame + 3 };
        test_true(sigpu_buffer_upload(buffer, 0, data, sizeof(data)));
        gpu_test_frames(1);
        test_uint(1, gpu_test_stats()->submissions);
        test_uint(frame + 1, gpu_test_session()->submissions);
    }
    uint32_t memory[4];
    test_true(gpu_test_read_buffer(buffer, 0, memory, sizeof(memory)));
    test_uint(5, memory[0]);
    test_uint(8, memory[3]);
    ecs_fini();
}

void frame_uploads_share_one_submission(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t buffer = gpu_test_buffer(16);
    ecs_entity_t texture = gpu_test_texture(2, 2);
    const uint32_t data[4] = { 1, 2, 3, 4 };
    test_true(sigpu_buffer_upload(buffer, 0, data, sizeof(data)));
    test_true(sigpu_texture_upload(texture, &(GpuTextureRegion){ .width = 2, .height = 2,
                                                                .depth = 1 }, data, 16, 0));
    gpu_test_frames(1);
    test_uint(2, gpu_test_stats()->uploads);
    test_uint(1, gpu_test_stats()->submissions);
    uint32_t pixels[4];
    test_true(gpu_test_read_texture(texture, 2, 2, pixels));
    test_uint(4, pixels[3]);
    ecs_fini();
}
