

#include "siecs.h"
#include <unistd.h>

int main(void) {
    ecs_world_t *world = ecs_init();

    while (true) {
        ecs_progress(world);
    }

    ecs_fini(world);
    return 0;
}
