

#include "siecs.h"
int main(void) {
    ecs_world_t *world = ecs_init();

    ecs_fini(world);
    return 0;
}
