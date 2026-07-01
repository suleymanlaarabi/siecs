#include "siecs.h"
#include "sijson.h"
#include "world_internal.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

ECS_COMPONENT(Position, {
    float x;
    float y;
});

ECS_COMPONENT(Velocity, {
    float x;
    float y;
});

ECS_COMPONENT(Girl, {});
ECS_COMPONENT(Boy, {});

int main(void) {
    ecs_world_t *world = ecs_init();

    ecs_component_t first = ECS_COMPONENT_REGISTER(world, Girl);
    ECS_COMPONENT_REGISTER(ecs_init(), Girl);

    assert(first == ecs_id(Girl));
}
