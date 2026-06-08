#include "siecs.h"
#include "storage/system_index.h"
#include "world_internal.h"
#include <string.h>

ecs_system_id_t ecs_system_init(ecs_world_t *world, const ecs_system_desc_t *desc) {
    ecs_system_t sys = {
        .qid = ecs_query_init(world, &desc->query),
        .callback = desc->callback,
    };

    memcpy(sys.after, desc->after, sizeof(ecs_system_id_t[4]));

    return ecs_system_index_create(&world->system_index, &sys);
}
