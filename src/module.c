#include "module.h"
#include "utils.h"
#include "world_internal.h"

static inline const void *ecs_module_key(const ecs_module_desc_t *desc) {
    return desc->key ? desc->key : (const void *)desc->import;
}

ecs_module_id_t ecs_module_init(ecs_world_t *world, const ecs_module_desc_t *desc) {
    ecs_assert_not_null(world);
    ecs_assert_not_null(desc);
    ecs_assert_not_null(desc->name);
    ecs_assert_not_null(desc->import);

    const void *key = ecs_module_key(desc);
    ecs_module_id_t existing = ecs_module_index_find(&world->module_index, key);
    if (existing) {
        return existing;
    }

    ecs_module_id_t module = ecs_module_index_create(&world->module_index, key, desc->name);
    ecs_module_id_t prev = world->active_module;
    world->active_module = module;
    desc->import(world, desc->desc);
    world->active_module = prev;

    if (desc->disabled) {
        ecs_module_disable(world, module);
    }

    return module;
}

ecs_module_id_t ecs_module_find(ecs_world_t *world, const void *key) {
    ecs_assert_not_null(world);
    return ecs_module_index_find(&world->module_index, key);
}

void ecs_module_enable(ecs_world_t *world, ecs_module_id_t module) {
    ecs_assert_not_null(world);

    ecs_module_t *record = ecs_module_index_get(&world->module_index, module);
    if (record->enabled) {
        return;
    }

    const ecs_system_id_t *systems = ecs_vec_data(&record->systems, ecs_system_id_t);
    for (uint32_t i = 0; i < record->systems.size; i++) {
        ecs_system_enable(world, systems[i]);
    }

    const ecs_observer_id_t *observers = ecs_vec_data(&record->observers, ecs_observer_id_t);
    for (uint32_t i = 0; i < record->observers.size; i++) {
        ecs_observer_enable(world, observers[i]);
    }

    record->enabled = true;
}

void ecs_module_disable(ecs_world_t *world, ecs_module_id_t module) {
    ecs_assert_not_null(world);

    ecs_module_t *record = ecs_module_index_get(&world->module_index, module);
    if (!record->enabled) {
        return;
    }

    const ecs_system_id_t *systems = ecs_vec_data(&record->systems, ecs_system_id_t);
    for (uint32_t i = 0; i < record->systems.size; i++) {
        ecs_system_disable(world, systems[i]);
    }

    const ecs_observer_id_t *observers = ecs_vec_data(&record->observers, ecs_observer_id_t);
    for (uint32_t i = 0; i < record->observers.size; i++) {
        ecs_observer_disable(world, observers[i]);
    }

    record->enabled = false;
}

bool ecs_module_is_enabled(const ecs_world_t *world, ecs_module_id_t module) {
    ecs_assert_not_null(world);
    return ecs_module_index_get_const(&world->module_index, module)->enabled;
}

void ecs_module_record_component(ecs_world_t *world, ecs_component_t component) {
    ecs_module_id_t module = world->active_module;
    if (!module) {
        return;
    }

    ecs_module_t *record = ecs_module_index_get(&world->module_index, module);
    ecs_vec_push_u16(&record->components, component);
}

void ecs_module_record_system(ecs_world_t *world, ecs_system_id_t system) {
    ecs_module_id_t module = world->active_module;
    if (!module) {
        return;
    }

    ecs_module_t *record = ecs_module_index_get(&world->module_index, module);
    ecs_vec_push_u16(&record->systems, system);
}

void ecs_module_record_observer(ecs_world_t *world, ecs_observer_id_t observer) {
    ecs_module_id_t module = world->active_module;
    if (!module) {
        return;
    }

    ecs_module_t *record = ecs_module_index_get(&world->module_index, module);
    ecs_vec_push(&record->observers, &observer, sizeof(ecs_observer_id_t));
}
