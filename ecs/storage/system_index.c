#include "system_index.h"
#include "ecs/datastructure/vec.h"

void ecs_system_index_init(ecs_system_index_t *index) {
    ecs_vec_init(&index->systems, sizeof(ecs_system_t));

    ecs_vec_ensure(&index->systems, 1, sizeof(ecs_system_t));
}

void ecs_system_index_fini(ecs_system_index_t *index) { ecs_vec_fini(&index->systems); }
