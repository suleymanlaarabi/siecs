#include "module_index.h"
#include "../utils.h"

static bool ecs_module_id_valid(const ecs_module_index_t *index, ecs_module_id_t module) {
    return module != 0 && module < index->modules.size;
}

static void ecs_module_record_init(ecs_module_t *module, const void *key, const char *name) {
    module->key = key;
    module->name = name;
    module->enabled = true;
    ecs_vec_init(&module->observers, sizeof(ecs_observer_id_t));
    ecs_vec_init(&module->systems, sizeof(ecs_system_id_t));
    ecs_vec_init(&module->components, sizeof(ecs_component_t));
}

static void ecs_module_record_fini(ecs_module_t *module) {
    ecs_vec_fini(&module->observers);
    ecs_vec_fini(&module->systems);
    ecs_vec_fini(&module->components);
}

void ecs_module_index_init(ecs_module_index_t *index) {
    ecs_vec_init(&index->modules, sizeof(ecs_module_t));
    ecs_vec_ensure(&index->modules, 1, sizeof(ecs_module_t));
}

void ecs_module_index_fini(ecs_module_index_t *index) {
    for (uint32_t i = 1; i < index->modules.size; i++) {
        ecs_module_t *module = ecs_vec_get_mut(&index->modules, i, ecs_module_t);
        ecs_module_record_fini(module);
    }
    ecs_vec_fini(&index->modules);
}

ecs_module_id_t ecs_module_index_create(
    ecs_module_index_t *index,
    const void *key,
    const char *name
) {
    ecs_module_t module;
    ecs_module_record_init(&module, key, name);
    ecs_vec_push(&index->modules, &module, sizeof(ecs_module_t));
    return index->modules.size - 1;
}

ecs_module_t *ecs_module_index_get(ecs_module_index_t *index, ecs_module_id_t module) {
    ecs_assert(ecs_module_id_valid(index, module), "invalid module id: %u\n", module);
    return ecs_vec_get_mut(&index->modules, module, ecs_module_t);
}

const ecs_module_t *ecs_module_index_get_const(
    const ecs_module_index_t *index,
    ecs_module_id_t module
) {
    ecs_assert(ecs_module_id_valid(index, module), "invalid module id: %u\n", module);
    return ecs_vec_get(&index->modules, module, ecs_module_t);
}

ecs_module_id_t ecs_module_index_find(const ecs_module_index_t *index, const void *key) {
    if (!key) {
        return 0;
    }

    for (uint32_t i = 1; i < index->modules.size; i++) {
        const ecs_module_t *module = ecs_vec_get(&index->modules, i, ecs_module_t);
        if (module->key == key) {
            return i;
        }
    }

    return 0;
}
