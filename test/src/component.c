#include "siecs.h"
#include "sijson.h"
#include <siecs_test.h>
#include <stdio.h>
#include <stdlib.h>

ECS_COMPONENT_DECLARE(Position, {
    float x;
    float y;
});

ECS_COMPONENT_DEFINE(Position);

void component_reflection(void) {
    ecs_world_t *world = ecs_init();
    ECS_COMPONENT_REGISTER(world, Position);

    ecs_entity_t entity = ecs_new(world);
    ecs_set(world, entity, Position, { 10, 20 });

    void *data = ecs_get(world, entity, Position);
    char *result = sijson_to_json_ptr(Position, data);

    test_str("{\"x\":10,\"y\":20}", result);
    puts(result);
    free(result);
    ecs_fini(world);
}
