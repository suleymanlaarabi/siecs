#include "siecs.h"
#include "storage/resource_index.h"
#include "utils.h"
#include "world_internal.h"

static ecs_resource_t ecs_resource_alloc_id(void) {
    ecs_resource_t id = ecs_world.resource_index.count;
    ecs_assert(id < UINT16_MAX, "resource id overflow\n");
    return id;
}

ecs_resource_t ecs_resource_init(const ecs_resource_desc_t *desc) {
    ecs_assert_not_scheduler_parallel("resource registration");
    return ecs_resource_index_register(ecs_resource_alloc_id(), desc);
}

ecs_resource_t ecs_resource_find(const char *name) { return ecs_resource_index_find(name); }

const char *ecs_resource_name(ecs_resource_t resource) {
    ecs_assert(ecs_resource_index_is_registered(resource), "invalid resource id: %u\n", resource);
    return ecs_world.resource_index.records[resource].name;
}

bool ecs_resource_is_registered_rid(ecs_resource_t id) {
    return ecs_resource_index_is_registered(id);
}

ecs_resource_t ecs_resource_register(ecs_resource_t *id, const ecs_resource_desc_t *desc) {
    ecs_assert_not_scheduler_parallel("resource registration");
    ecs_assert_not_null(id);
    if (*id == 0) {
        *id = ecs_resource_alloc_id();
    }
    return ecs_resource_index_register(*id, desc);
}

void ecs_set_resource_rid(ecs_resource_t id, const void *data) {
    ecs_assert_id_valid(id);
    ecs_assert_not_null(data);

    ecs_resource_index_set(id, data);
}

void ecs_move_resource_rid(ecs_resource_t id, void *data) {
    ecs_assert_id_valid(id);
    ecs_assert_not_null(data);

    ecs_resource_index_move(id, data);
}

void *ecs_resource_rid(ecs_resource_t id) {
    ecs_assert_id_valid(id);

    void *resource = ecs_resource_index_get(id);
    ecs_assert(resource != NULL, "resource does not exist: %d\n", id);
    return resource;
}

void *ecs_try_resource_rid(ecs_resource_t id) {
    ecs_assert_id_valid(id);

    return ecs_resource_index_get(id);
}

bool ecs_has_resource_rid(const ecs_resource_t id) {
    ecs_assert_id_valid(id);

    return ecs_resource_index_has(id);
}

void ecs_remove_resource_rid(ecs_resource_t id) {
    ecs_assert_id_valid(id);

    ecs_resource_index_remove(id);
}
