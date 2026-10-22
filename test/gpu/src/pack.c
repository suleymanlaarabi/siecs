#include "util.h"
#include <string.h>

void pack_truncated_pack_fails(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    size_t size = 0;
    const unsigned char *pack = gpu_test_pack(&size);
    test_true(sigpu_asset_register("truncated", pack, size / 2));
    test_true(sigpu_asset_register("tiny", pack, 8));
    ecs_entity_t truncated = gpu_test_shader("truncated");
    ecs_entity_t tiny = gpu_test_shader("tiny");
    gpu_test_frames(1);
    test_uint(GpuErrorPack, gpu_test_state(truncated)->error);
    test_assert(strstr(gpu_test_state(truncated)->message, "size mismatch") != NULL);
    test_uint(GpuErrorPack, gpu_test_state(tiny)->error);
    test_assert(strstr(gpu_test_state(tiny)->message, "truncated header") != NULL);
    ecs_fini();
}

void pack_fingerprint_mismatch_fails(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    size_t size = 0;
    const unsigned char *pack = gpu_test_pack(&size);
    const uint64_t fingerprint = gpu_pack_fingerprint(pack, size);
    ecs_entity_t shader = ecs_new();
    ecs_set(shader, GpuShader, { .path = TEST_PACK, .fingerprint = fingerprint + 1 });
    gpu_test_frames(1);
    test_uint(GpuErrorPack, gpu_test_state(shader)->error);
    test_assert(strstr(gpu_test_state(shader)->message, "fingerprint") != NULL);

    ecs_set(shader, GpuShader, { .path = TEST_PACK, .fingerprint = fingerprint });
    gpu_test_frames(1);
    test_true(sigpu_ready(shader));
    ecs_fini();
}
