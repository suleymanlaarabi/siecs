#include "module_index.h"
#include "../utils.h"
#include "../world_internal.h"

static bool ecs_module_id_valid(const ecs_module_index_t *index, ecs_module_id_t module) {
    return module != 0 && module < index->modules.size;
}

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
    ecs_module_index_t *index = &ecs_world.module_index;
    sicore_vec_init(&index->modules, sizeof(ecs_module_t));
    sicore_vec_ensure(&index->modules, 1, sizeof(ecs_module_t));
}

void ecs_module_index_fini() {
    ecs_module_index_t *index = &ecs_world.module_index;
    for (uint32_t i = 1; i < index->modules.size; i++) {
        ecs_module_t *module = sicore_vec_get_mut(&index->modules, i, ecs_module_t);
        ecs_module_record_fini(module);
    }
    sicore_vec_fini(&index->modules);
}

ecs_module_id_t ecs_module_index_create(ecs_module_id_t *id, const char *name) {
    ecs_module_index_t *index = &ecs_world.module_index;
    ecs_module_t module;
    ecs_module_record_init(&module, id, name);
    sicore_vec_push(&index->modules, &module, sizeof(ecs_module_t));
    return index->modules.size - 1;
}

ecs_module_t *ecs_module_index_get(ecs_module_id_t module) {
    ecs_module_index_t *index = &ecs_world.module_index;
    ecs_assert(ecs_module_id_valid(index, module), "invalid module id: %u\n", module);
    return sicore_vec_get_mut(&index->modules, module, ecs_module_t);
}

const ecs_module_t *ecs_module_index_get_const(ecs_module_id_t module) {
    const ecs_module_index_t *index = &ecs_world.module_index;
    ecs_assert(ecs_module_id_valid(index, module), "invalid module id: %u\n", module);
    return sicore_vec_get(&index->modules, module, ecs_module_t);
}

ecs_module_id_t ecs_module_index_find(const ecs_module_id_t *id) {
    const ecs_module_index_t *index = &ecs_world.module_index;
    if (!id || !*id) {
        return 0;
    }

    ecs_module_id_t module = *id;
    if (ecs_module_id_valid(index, module)) {
        return module;
    }

    return 0;
}
