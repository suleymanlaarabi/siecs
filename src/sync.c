#include "world_internal.h"

typedef struct {
    ecs_sync_callback_t callback;
    void *context;
    uint32_t generation;
} ecs_sync_entry_t;

/* A process-wide generation prevents an old-world handle from addressing a
 * callback registered after init/fini. Registry writes are main-thread only. */
static uint32_t sync_next_generation;

ecs_sync_id_t ecs_sync_register(ecs_sync_callback_t callback, void *context) {
    ecs_assert_not_scheduler_parallel("synchronization registration");
    ecs_assert(callback != NULL, "synchronization callback is required");
    if (!ecs_world.sync_callbacks.data)
        sicore_vec_init(&ecs_world.sync_callbacks, sizeof(ecs_sync_entry_t));
    ecs_sync_entry_t entry = { callback, context, ++sync_next_generation };
    ecs_assert(entry.generation != 0, "synchronization handle space exhausted");
    for (uint32_t i = 0; i < ecs_world.sync_callbacks.size; i++) {
        ecs_sync_entry_t *slot = sicore_vec_get_mut(&ecs_world.sync_callbacks, i, ecs_sync_entry_t);
        if (!slot->callback) {
            *slot = entry;
            return ((uint64_t)entry.generation << 32) | (i + 1);
        }
    }
    sicore_vec_push(&ecs_world.sync_callbacks, &entry, sizeof entry);
    return ((uint64_t)entry.generation << 32) | ecs_world.sync_callbacks.size;
}

void ecs_sync_unregister(ecs_sync_id_t id) {
    ecs_assert_not_scheduler_parallel("synchronization removal");
    uint32_t index = (uint32_t)id - 1;
    if (index >= ecs_world.sync_callbacks.size)
        return;
    ecs_sync_entry_t *entry =
        sicore_vec_get_mut(&ecs_world.sync_callbacks, index, ecs_sync_entry_t);
    if (entry->generation == (uint32_t)(id >> 32))
        entry->callback = NULL;
}

void ecs_sync(void) {
    if (!ecs_world.sync_callbacks.size || ecs_world.sync_depth || ecs_world.synchronizing ||
        ecs_is_deferred())
        return;
    ecs_assert_not_scheduler_parallel("world synchronization");
    ecs_world.synchronizing = true;
    uint32_t count = ecs_world.sync_callbacks.size;
    for (uint32_t i = 0; i < count; i++) {
        ecs_sync_entry_t entry = *sicore_vec_get(&ecs_world.sync_callbacks, i, ecs_sync_entry_t);
        if (entry.callback)
            entry.callback(entry.context);
    }
    ecs_world.synchronizing = false;
}

void ecs_sync_begin(void) {
    ecs_execution_context_t *context = ecs_execution_context_current();
    if (!context->scheduler_parallel)
        ecs_world.sync_depth++;
}

void ecs_sync_end(void) {
    ecs_execution_context_t *context = ecs_execution_context_current();
    if (context->scheduler_parallel)
        return;
    ecs_assert(ecs_world.sync_depth != 0, "unbalanced synchronization boundary");
    ecs_world.sync_depth--;
    ecs_sync();
}

void ecs_sync_storage_fini(void) { sicore_vec_fini(&ecs_world.sync_callbacks); }
