#include "util.h"
#include <string.h>

void state_descriptor_adds_state(void) {
    gpu_test_start(0);
    ecs_entity_t buffer = gpu_test_buffer(16);
    test_true(ecs_has(buffer, GpuResourceState));
    const GpuResourceState *state = gpu_test_state(buffer);
    test_uint(GpuPending, state->status);
    test_uint(1, state->requested);
    test_uint(0, state->applied);
    test_uint(0, state->record);
    ecs_fini();
}

void state_ready_after_frame(void) {
    gpu_test_start(0);
    ecs_entity_t device = gpu_test_device(0);
    ecs_entity_t buffer = gpu_test_buffer(16);
    gpu_test_frames(1);
    test_uint(GpuReady, gpu_test_state(device)->status);
    const GpuResourceState *state = gpu_test_state(buffer);
    test_uint(GpuReady, state->status);
    test_uint(1, state->applied);
    test_uint(1, state->allocation);
    test_true(sigpu_ready(buffer));
    test_uint(1, gpu_test_stats()->created);
    test_uint(1, gpu_test_stats()->objects);
    test_assert(gpu_resource_buffer(buffer) != NULL);

    const GpuSession *session = gpu_test_session();
    test_uint(device, session->device);
    test_true(session->ready);
    test_uint(2, session->frames_in_flight);
    test_assert(session->driver[0] != '\0');
    ecs_fini();
}

void state_isa_instance_starts_pending(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t base = gpu_test_buffer(16);
    gpu_test_frames(1);
    test_true(sigpu_ready(base));

    ecs_entity_t instance = ecs_new();
    ecs_is_a(instance, base);
    test_true(ecs_has_cid_owned(instance, ecs_id(GpuResourceState)));
    const GpuResourceState *state = gpu_test_state(instance);
    test_uint(GpuPending, state->status);
    test_uint(0, state->record);
    test_true(sigpu_ready(base));

    gpu_test_frames(1);
    test_true(sigpu_ready(instance));
    test_assert(gpu_resource_buffer(instance) != gpu_resource_buffer(base));
    test_uint(2, gpu_test_stats()->objects);
    ecs_fini();
}

void state_isa_objects_released_once(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t base = gpu_test_buffer(16);
    gpu_test_frames(1);
    ecs_entity_t instance = ecs_new();
    ecs_is_a(instance, base);
    gpu_test_frames(1);
    test_uint(2, gpu_test_stats()->objects);

    ecs_kill(instance);
    ecs_kill(base);
    gpu_test_frames(1);
    test_uint(2, gpu_test_stats()->released);
    test_uint(0, gpu_test_stats()->objects);
    ecs_fini();
}

ECS_TAG(Unrelated);

void state_migration_keeps_gpu_objects(void) {
    gpu_test_start(0);
    gpu_test_device(0);
    ecs_entity_t buffer = gpu_test_buffer(16);
    gpu_test_frames(1);
    SDL_GPUBuffer *object = gpu_resource_buffer(buffer);

    /* Moving the entity to another table must move its state as is. */
    ECS_COMPONENT_REGISTER(Unrelated);
    ecs_add(buffer, Unrelated);
    test_true(sigpu_ready(buffer));
    test_assert(gpu_resource_buffer(buffer) == object);
    ecs_remove(buffer, Unrelated);
    gpu_test_frames(1);
    test_true(sigpu_ready(buffer));
    test_assert(gpu_resource_buffer(buffer) == object);
    test_uint(1, gpu_test_state(buffer)->allocation);
    test_uint(0, gpu_test_stats()->released);
    test_uint(1, gpu_test_stats()->objects);
    ecs_fini();
}

static ecs_entity_t watched;
static uint32_t transitions[GpuRetiring + 1];

static void count_transition(ecs_observer_event_t *event) {
    if (event->entity == watched && event->component == ecs_id(GpuResourceState)) {
        transitions[((const GpuResourceState *)event->trigger_data)->status]++;
    }
}

void state_observer_sees_transitions(void) {
    gpu_test_start(0);
    memset(transitions, 0, sizeof(transitions));
    ecs_observer({ .on = EcsOnSet, .query.components = { ecs_filter(GpuResourceState) },
                   .callback = count_transition });
    gpu_test_device(0);
    watched = ecs_new();
    ecs_set(watched, GpuBuffer, { .size = 6, .usage = GpuBufferVertex });
    gpu_test_frames(1);
    test_uint(1, transitions[GpuFailed]);
    ecs_set(watched, GpuBuffer, { .size = 8, .usage = GpuBufferVertex });
    gpu_test_frames(1);
    test_uint(1, transitions[GpuReady]);
    /* Nothing changed: no new notification. */
    gpu_test_frames(2);
    test_uint(1, transitions[GpuReady]);
    test_uint(1, transitions[GpuFailed]);
    ecs_fini();
}
