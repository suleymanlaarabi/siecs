#include "siecs.h"
#include "storage/resource_index.h"
#include "utils.h"
#include "world_internal.h"

ecs_resource_t ecs_resource_init(ecs_world_t *world, const ecs_resource_desc_t *desc) {
    ecs_assert_not_null(world);
    return ecs_resource_index_register(&world->resource_index, desc);
}

void ecs_set_resource_rid(ecs_world_t *world, ecs_resource_t id, const void *data) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(id);
    ecs_assert_not_null(data);

    ecs_resource_index_set(&world->resource_index, world, id, data);
}

void *ecs_resource_rid(ecs_world_t *world, ecs_resource_t id) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(id);

    void *resource = ecs_resource_index_get(&world->resource_index, id);
    ecs_assert(resource != NULL, "resource does not exist: %d\n", id);
    return resource;
}

void *ecs_try_resource_rid(ecs_world_t *world, ecs_resource_t id) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(id);

    return ecs_resource_index_get(&world->resource_index, id);
}

bool ecs_has_resource_rid(const ecs_world_t *world, ecs_resource_t id) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(id);

    return ecs_resource_index_has(&world->resource_index, id);
}

void ecs_remove_resource_rid(ecs_world_t *world, ecs_resource_t id) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(id);

    ecs_resource_index_remove(&world->resource_index, world, id);
}
