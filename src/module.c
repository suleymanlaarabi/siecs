#include "module.h"
#include "platform.h"
#include "storage/query_index.h"
#include "utils.h"
#include "world_internal.h"
#include <stdlib.h>
#include <string.h>

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
    }

    for (uint32_t i = ecs_modules.size; i > 1; i--) {
        ecs_module_t *module = &modules[i - 1];

        if (module->library) {
            ecs_platform_library_close(module->library);
        }

        free(module->owned_name);
    }

    sicore_vec_fini(&ecs_modules);
}

static ecs_module_id_t ecs_module_begin(
    ecs_module_t record,
    ecs_module_id_t *previous
) {
    sicore_vec_push(&ecs_modules, &record, sizeof(record));

    ecs_module_id_t module = (ecs_module_id_t)(ecs_modules.size - 1);

    *previous = ecs_world.active_module;
    ecs_world.active_module = module;

    return module;
}

static void ecs_module_end(ecs_module_id_t previous) {
    ecs_world.active_module = previous;
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
        .owned_name = NULL,
        .library = NULL,
        .observer = UINT32_MAX,
        .system = UINT16_MAX,
        .enabled = true,
    };

    ecs_module_id_t previous;
    ecs_module_id_t module = ecs_module_begin(record, &previous);

    if (desc->id) {
        *desc->id = module;
    }

    desc->import(desc->desc);

    ecs_module_end(previous);

    if (desc->disabled) {
        ecs_module_disable(module);
    }
    return module;
}

static char *ecs_module_path_name(const char *path) {
    const char *name = path;

    for (const char *cursor = path; *cursor; cursor++) {
        if (*cursor == '/' || *cursor == '\\') {
            name = cursor + 1;
        }
    }

    size_t length = strlen(name);
    char *copy = malloc(length + 1);

    if (!copy) {
        abort();
    }

    memcpy(copy, name, length + 1);
    return copy;
}

static char *ecs_module_library_path(const char *path) {
#ifdef _WIN32
    static const char suffix[] = ".dll";
#elif defined(__APPLE__)
    static const char suffix[] = ".dylib";
#elif defined(__EMSCRIPTEN__)
    static const char suffix[] = "";
#else
    static const char suffix[] = ".so";
#endif

    size_t path_length = strlen(path);
    size_t suffix_length = sizeof(suffix) - 1;

    char *result = malloc(path_length + suffix_length + 1);

    if (!result) {
        abort();
    }

    memcpy(result, path, path_length);
    memcpy(result + path_length, suffix, suffix_length + 1);

    return result;
}

ecs_module_id_t ecs_module_load(const char *path) {
    ecs_assert_not_scheduler_parallel("module loading");
    ecs_assert_not_null(path);

#ifdef __EMSCRIPTEN__
    (void)path;
    return 0;
#else
    char *library_path = ecs_module_library_path(path);
    ecs_platform_library_t library = ecs_platform_library_open(library_path);
    free(library_path);

    if (library == NULL) {
        return 0;
    }

    ecs_module_t *modules = ecs_modules.data;

    for (uint32_t i = 1; i < ecs_modules.size; i++) {
        if (modules[i].library == library) {
            ecs_platform_library_close(library);
            return (ecs_module_id_t)i;
        }
    }

    ecs_module_dynamic_import_t import;
    {
        void *symbol =
            ecs_platform_library_symbol(library, "ecs_module_import");

        if (!symbol) {
            ecs_platform_library_close(library);
            return 0;
        }

        memcpy(&import, &symbol, sizeof(import));
    }

    char *name = ecs_module_path_name(path);

    ecs_module_t record = {
        .id = NULL,
        .name = name,
        .owned_name = name,
        .library = library,
        .observer = UINT32_MAX,
        .system = UINT16_MAX,
        .enabled = true,
    };

    ecs_module_id_t previous;
    ecs_module_id_t module = ecs_module_begin(record, &previous);

    import();

    ecs_module_end(previous);

    return module;
#endif
}

static void ecs_module_set_enabled(ecs_module_id_t module, bool enabled) {
    ecs_module_t *record = ecs_module_record(module);
    if (record->enabled == enabled) return;
    for (ecs_system_id_t id = record->system; id != UINT16_MAX;
         id = ecs_system_index_get(id)->next_module) {
        if (enabled) ecs_system_enable(id); else ecs_system_disable(id);
    }
    for (ecs_observer_id_t id = record->observer; id != UINT32_MAX;
         id = sicore_vec_get(&observer_index.observers, id, ecs_observer_t)->next_module) {
        if (enabled) ecs_observer_enable(id); else ecs_observer_disable(id);
    }
    record->enabled = enabled;
}

void ecs_module_enable(ecs_module_id_t module) { ecs_module_set_enabled(module, true); }

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
    ecs_module_set_enabled(module, false);
}

bool ecs_module_is_enabled(const ecs_module_id_t module) {
    return ecs_module_record(module)->enabled;
}

void ecs_module_record_system(ecs_system_id_t system) {
    ecs_module_id_t module = ecs_world.active_module;
    if (module) {
        ecs_system_t *value = ecs_system_index_get(system);
        value->next_module = ecs_module_record(module)->system;
        ecs_module_record(module)->system = system;
    }
}

void ecs_module_record_observer(ecs_observer_id_t observer) {
    ecs_module_id_t module = ecs_world.active_module;
    if (module) {
        ecs_observer_t *value =
            sicore_vec_get_mut(&observer_index.observers, observer, ecs_observer_t);
        value->next_module = ecs_module_record(module)->observer;
        ecs_module_record(module)->observer = observer;
    }
}
