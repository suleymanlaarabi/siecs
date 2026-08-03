#include "module.h"
#include "utils.h"
#include "world_internal.h"

ecs_module_id_t ecs_module_init(const ecs_module_desc_t *desc) {
    ecs_assert_not_null(desc);
    ecs_assert_not_null(desc->name);
    ecs_assert_not_null(desc->import);

    ecs_module_id_t existing = ecs_module_index_find(desc->id);
    if (existing) {
        return existing;
    }

    ecs_module_id_t module = ecs_module_index_create(desc->id, desc->name);
    if (desc->id) {
        *desc->id = module;
    }

    ecs_module_id_t prev = ecs_world.active_module;
    ecs_world.active_module = module;
    desc->import(desc->desc);
    ecs_world.active_module = prev;

    if (desc->disabled) {
        ecs_module_disable(module);
    }

    return module;
}

void ecs_module_enable(ecs_module_id_t module) {

    ecs_module_t *record = ecs_module_index_get(module);
    if (record->enabled) {
        return;
    }

    const ecs_system_id_t *systems = sicore_vec_data(&record->systems, ecs_system_id_t);
    for (uint32_t i = 0; i < record->systems.size; i++) {
        ecs_system_enable(systems[i]);
    }

    const ecs_observer_id_t *observers = sicore_vec_data(&record->observers, ecs_observer_id_t);
    for (uint32_t i = 0; i < record->observers.size; i++) {
        ecs_observer_enable(observers[i]);
    }

    record->enabled = true;
}

ecs_module_id_t ecs_module_find(const ecs_module_id_t *id) { return ecs_module_index_find(id); }

const char *ecs_module_name(ecs_module_id_t module) {
    return ecs_module_index_get_const(module)->name;
}

void ecs_module_disable(ecs_module_id_t module) {

    ecs_module_t *record = ecs_module_index_get(module);
    if (!record->enabled) {
        return;
    }

    const ecs_system_id_t *systems = sicore_vec_data(&record->systems, ecs_system_id_t);
    for (uint32_t i = 0; i < record->systems.size; i++) {
        ecs_system_disable(systems[i]);
    }

    const ecs_observer_id_t *observers = sicore_vec_data(&record->observers, ecs_observer_id_t);
    for (uint32_t i = 0; i < record->observers.size; i++) {
        ecs_observer_disable(observers[i]);
    }

    record->enabled = false;
}

bool ecs_module_is_enabled(const ecs_module_id_t module) {
    return ecs_module_index_get_const(module)->enabled;
}

void ecs_module_record_system(ecs_system_id_t system) {
    ecs_module_id_t module = ecs_world.active_module;
    if (!module) {
        return;
    }

    ecs_module_t *record = ecs_module_index_get(module);
    sicore_vec_push_u16(&record->systems, system);
}

void ecs_module_record_observer(ecs_observer_id_t observer) {
    ecs_module_id_t module = ecs_world.active_module;
    if (!module) {
        return;
    }

    ecs_module_t *record = ecs_module_index_get(module);
    sicore_vec_push(&record->observers, &observer, sizeof(ecs_observer_id_t));
}
