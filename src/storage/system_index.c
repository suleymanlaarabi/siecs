#include "system_index.h"
#include "../datastructure/vec.h"

void ecs_system_index_init(ecs_system_index_t *index) {
    ecs_vec_init(&index->systems, sizeof(ecs_system_t));

    ecs_vec_ensure(&index->systems, 1, sizeof(ecs_system_t));
}

ecs_system_id_t ecs_system_index_create(ecs_system_index_t *index, const ecs_system_t *system) {
    ecs_vec_push(&index->systems, system, sizeof(ecs_system_t));
    return index->systems.size - 1;
}

void ecs_system_index_fini(ecs_system_index_t *index) { ecs_vec_fini(&index->systems); }
