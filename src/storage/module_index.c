#include "module_index.h"
#include "index_vec.h"
#include "../utils.h"
#include "../world_internal.h"

ecs_module_index_t module_index;

ECS_INDEX_VEC_ID_VALID(
    ecs_module_id_valid,
    ecs_module_id_t,
    module_index.modules
)

ECS_INDEX_VEC_GET_MUT(
    ecs_module_get_unchecked,
    ecs_module_id_t,
    ecs_module_t,
    module_index.modules
)

ECS_INDEX_VEC_GET(
    ecs_module_get_const_unchecked,
    ecs_module_id_t,
    ecs_module_t,
    module_index.modules
)

static void ecs_module_record_init(ecs_module_t *module, ecs_module_id_t *id, const char *name) {
    module->id = id;
    module->name = name;
    module->enabled = true;
    sicore_vec_init(&module->observers, sizeof(ecs_observer_id_t));
    sicore_vec_init(&module->systems, sizeof(ecs_system_id_t));
}

static void ecs_module_record_fini(ecs_module_t *module) {
    if (module->id) {
        *module->id = 0;
    }

    sicore_vec_fini(&module->observers);
    sicore_vec_fini(&module->systems);
}

void ecs_module_index_init() {
    ecs_module_index_t *index = &module_index;
    sicore_vec_init(&index->modules, sizeof(ecs_module_t));
    sicore_vec_ensure(&index->modules, 1, sizeof(ecs_module_t));
}

void ecs_module_index_fini() {
    ecs_module_index_t *index = &module_index;
    for (uint32_t i = 1; i < index->modules.size; i++) {
        ecs_module_t *module = ecs_module_get_unchecked((ecs_module_id_t)i);
        ecs_module_record_fini(module);
    }
    sicore_vec_fini(&index->modules);
    *index = (ecs_module_index_t){ 0 };
}

ecs_module_id_t ecs_module_index_create(ecs_module_id_t *id, const char *name) {
    ecs_module_index_t *index = &module_index;
    ecs_module_t module;
    ecs_module_record_init(&module, id, name);
    sicore_vec_push(&index->modules, &module, sizeof(ecs_module_t));
    return index->modules.size - 1;
}

ecs_module_t *ecs_module_index_get(ecs_module_id_t module) {
    ecs_assert(ecs_module_id_valid(module), "invalid module id: %u\n", module);
    return ecs_module_get_unchecked(module);
}

const ecs_module_t *ecs_module_index_get_const(ecs_module_id_t module) {
    ecs_assert(ecs_module_id_valid(module), "invalid module id: %u\n", module);
    return ecs_module_get_const_unchecked(module);
}

ecs_module_id_t ecs_module_index_find(const ecs_module_id_t *id) {
    if (!id || !*id) {
        return 0;
    }

    ecs_module_id_t module = *id;
    if (ecs_module_id_valid(module)) {
        return module;
    }

    return 0;
}
