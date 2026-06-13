#include "siecs.h"
#include <unistd.h>

int main(void) {
    ecs_world_t *world = ecs_with_features({ .rest = true });

    ecs_new(world);

    ecs_entity_t parent = ecs_new(world);

    ecs_set(world, parent, Name, { "Parent" });

    ecs_entity_t child = ecs_new(world);
    ecs_set(world, child, Name, { "Child" });
    ecs_set(world, child, ChildOf, { parent });

    while (ecs_progress(world)) {
    }

    ecs_fini(world);
    return 0;
}
