#include "module.h"
#include "storage/query_index.h"
#include "utils.h"
#include "world_internal.h"

static sicore_vec_t ecs_modules;

void ecs_module_storage_init(void) {
    sicore_vec_init(&ecs_modules, sizeof(ecs_module_t));
    sicore_vec_ensure(&ecs_modules, 1, sizeof(ecs_module_t));
}

static inline ecs_module_t *ecs_module_record(ecs_module_id_t id) {
    ecs_assert(id != 0 && id < ecs_modules.size, "invalid module id: %u\n", id);
    return sicore_vec_get_mut(&ecs_modules, id, ecs_module_t);
}

void ecs_module_storage_fini(void) {
    ecs_module_t *modules = ecs_modules.data;
    for (uint32_t i = 1; i < ecs_modules.size; i++) {
        if (modules[i].id) {
            *modules[i].id = 0;
        }
        sicore_vec_fini(&modules[i].observers);
        sicore_vec_fini(&modules[i].systems);
    }
    sicore_vec_fini(&ecs_modules);
}

ecs_module_id_t ecs_module_init(const ecs_module_desc_t *desc) {
    ecs_assert_not_scheduler_parallel("module registration");
    ecs_assert_not_null(desc);
    ecs_assert_not_null(desc->name);
    ecs_assert_not_null(desc->import);

    ecs_module_id_t existing =
        desc->id && *desc->id && *desc->id < ecs_modules.size ? *desc->id : 0;
    if (existing) {
        return existing;
    }

    ecs_module_t record = {
        .id = desc->id,
        .name = desc->name,
        .enabled = true,
    };
    sicore_vec_init(&record.observers, sizeof(ecs_observer_id_t));
    sicore_vec_init(&record.systems, sizeof(ecs_system_id_t));
    sicore_vec_push(&ecs_modules, &record, sizeof(record));

    ecs_module_id_t module = (ecs_module_id_t)(ecs_modules.size - 1);
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
    ecs_module_t *record = ecs_module_record(module);
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

ecs_module_id_t ecs_module_find(const ecs_module_id_t *id) {
    if (!id || !*id || *id >= ecs_modules.size) {
        return 0;
    }
    return *id;
}

const char *ecs_module_name(ecs_module_id_t module) {
    return ecs_module_record(module)->name;
}

void ecs_module_disable(ecs_module_id_t module) {
    ecs_module_t *record = ecs_module_record(module);
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
    return ecs_module_record(module)->enabled;
}

void ecs_module_record_system(ecs_system_id_t system) {
    ecs_module_id_t module = ecs_world.active_module;
    if (module) {
        sicore_vec_push_u16(&ecs_module_record(module)->systems, system);
    }
}

void ecs_module_record_observer(ecs_observer_id_t observer) {
    ecs_module_id_t module = ecs_world.active_module;
    if (module) {
        sicore_vec_push(&ecs_module_record(module)->observers, &observer, sizeof(observer));
    }
}
