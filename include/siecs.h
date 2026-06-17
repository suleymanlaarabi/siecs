#ifndef SIECS_H
#define SIECS_H

#include "siecs/bake_config.h"

#ifdef __cplusplus
#ifndef _Alignof
#define _Alignof alignof
#endif
#endif

#include <sireflect.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Public id symbol generated for a component type.
 *
 * Users normally do not call this macro directly. It is used by the typed
 * helpers such as ecs_add(world, entity, Position).
 */
#define ecs_id(name) _ecs_id_##name##__

/* Opaque ECS world. Create with ecs_init and destroy with ecs_fini. */
struct ecs_world_s;
typedef struct ecs_world_s ecs_world_t;

/* Public handle types. A zero id is reserved internally and is not user data.
 */
typedef uint64_t ecs_entity_t;
typedef uint16_t ecs_component_t;
typedef uint16_t ecs_query_id_t;
typedef uint16_t ecs_system_id_t;
typedef uint16_t ecs_event_t;
typedef uint16_t ecs_module_id_t;
typedef uint32_t ecs_observer_id_t;

/*
 * Module import callback.
 *
 * Called the first time the module is imported into the active world. The
 * callback is expected to register the module's components, systems, observers,
 * and nested modules. Registrations made while the callback runs are recorded
 * under the importing module so ecs_module_enable/ecs_module_disable can toggle
 * its systems and observers later.
 *
 * desc is the user props pointer from ecs_module_desc_t. The library does not
 * retain or copy it; it is only valid for the duration of this call.
 */
typedef void (*ecs_module_import_t)(ecs_world_t *world, const void *desc);

/*
 * Module registration descriptor.
 *
 * name and import are required.
 *
 * id points to the module id storage used as the import cache. If *id is not 0,
 * ecs_module_init returns it and does not call import again. If *id is 0,
 * ecs_module_init creates the module and stores the new id in *id. The typed
 * ECS_MODULE_* macros pass the generated ecs_id(module_name) storage here.
 *
 * This module cache is process-global by design. SIECS supports one active
 * world owning a given typed module at a time; ecs_fini resets imported module
 * ids to 0 so a later world can import them again.
 *
 * desc/desc_size describe optional import props. desc_size is informational in
 * the current C API and is kept for validation/introspection compatibility.
 *
 * disabled imports the module and immediately disables the systems and observers
 * captured during import. Components remain registered.
 */
typedef struct {
    const char *name;
    ecs_module_id_t *id;
    ecs_module_import_t import;
    const void *desc;
    uint32_t desc_size;
    bool disabled;
} ecs_module_desc_t;

/*
 * Event payload passed to observer callbacks.
 *
 * trigger_data is event-specific:
 * - OnAdd: pointer to the added component storage.
 * - OnRemove: pointer to the component storage before removal.
 * - OnSet: pointer to the new value passed to ecs_set/ecs_set_cid.
 * - Custom events: pointer passed to ecs_observer_trigger.
 */
typedef struct {
    ecs_world_t *world;
    ecs_entity_t entity;
    ecs_event_t event;
    uintptr_t user_data;
    const void *trigger_data;
} ecs_observer_event_t;

typedef void (*ecs_observer_callback_t)(ecs_observer_event_t *event);

/*
 * Component lifecycle hook.
 *
 * on_set receives the new value passed to ecs_set/ecs_set_cid. At that moment,
 * the table still contains the previous value, so ecs_get/ecs_get_cid can be
 * used by the hook to inspect old data.
 *
 * on_remove receives the value that is about to be removed.
 */
typedef void (*ecs_component_hook_t)(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t component,
    const void *ptr
);

/*
 * Component registration descriptor.
 *
 * name and size are required for normal components. Hooks are optional.
 * Relation fields are used by ECS_RELATION_DEFINE and should normally not be
 * filled manually by user code.
 */
typedef struct {
    const char *name;
    uint64_t size;
    ecs_component_hook_t on_set;
    ecs_component_hook_t on_remove;
    ecs_component_hook_t on_add;
    bool is_relation;
    const sireflect_struct_desc_t *struct_desc;
} ecs_component_desc_t;

/* Query term access mode. */
typedef enum {
    EcsIn,     /* Component must exist and is returned by ecs_field for reading. */
    EcsOut,    /* Component must exist and is returned by ecs_field for writing. */
    EcsInOut,  /* Component must exist and is returned by ecs_field for read/write. */
    EcsFilter, /* Component must exist but is not returned by ecs_field. */
    EcsNot,    /* Component must not exist and is not returned by ecs_field. */
} ecs_term_access_t;

typedef struct {
    ecs_component_t id;
    ecs_term_access_t access;
} ecs_query_term_t;

/*
 * Query descriptor.
 *
 * terms is a zero-terminated component term list. Terms with EcsIn, EcsOut and
 * EcsInOut are returned by ecs_field in declaration order. EcsFilter and EcsNot
 * only affect table matching.
 *
 * A query must contain at least one term.
 */
typedef struct {
    ecs_query_term_t terms[16];
} ecs_query_desc_t;

#ifdef __cplusplus
#define ecs_in(cname)                                                                              \
    ecs_query_term_t { ecs_id(cname), EcsIn }
#define ecs_out(cname)                                                                             \
    ecs_query_term_t { ecs_id(cname), EcsOut }
#define ecs_inout(cname)                                                                           \
    ecs_query_term_t { ecs_id(cname), EcsInOut }
#define ecs_filter(cname)                                                                          \
    ecs_query_term_t { ecs_id(cname), EcsFilter }
#define ecs_not(cname)                                                                             \
    ecs_query_term_t { ecs_id(cname), EcsNot }
#else
#define ecs_in(cname) ((ecs_query_term_t){ ecs_id(cname), EcsIn })
#define ecs_out(cname) ((ecs_query_term_t){ ecs_id(cname), EcsOut })
#define ecs_inout(cname) ((ecs_query_term_t){ ecs_id(cname), EcsInOut })
#define ecs_filter(cname) ((ecs_query_term_t){ ecs_id(cname), EcsFilter })
#define ecs_not(cname) ((ecs_query_term_t){ ecs_id(cname), EcsNot })
#endif

/* Create an ECS world. */
ecs_world_t *ecs_init(void);

/* World feature descriptor. */
typedef struct {
    bool rest;
} ecs_world_feat_desc_t;

/* Create a world with the given features. */
#define ecs_with_features(...) ecs_init_w_features(&(ecs_world_feat_desc_t)__VA_ARGS__)

/* Initialize a world with the given features. */
ecs_world_t *ecs_init_w_features(const ecs_world_feat_desc_t *features);

/* Destroy a world and all ECS storage owned by it. world must not be NULL. */
void ecs_fini(ecs_world_t *world);

/*
 * Declare a component type and its public component id.
 *
 * Use in headers:
 *   ECS_COMPONENT_DECLARE(Position, { float x; float y; });
 */
#define ECS_COMPONENT_DECLARE(cname, ...)                                                          \
    SIJSON_DECLARE(cname, __VA_ARGS__)                                                             \
    extern ecs_component_t ecs_id(cname);                                                          \
    extern ecs_component_desc_t ecs_id(cname##_desc)

/*
 * Define a component declared with ECS_COMPONENT_DECLARE.
 *
 * Use once in a C file:
 *   ECS_COMPONENT_DEFINE(Position);
 */
#define ECS_COMPONENT_DEFINE(cname, ...)                                                           \
    SIJSON_DEFINE(cname)                                                                           \
    ecs_component_desc_t ecs_id(cname##_desc) = { .name = #cname,                                  \
                                                  .size = sizeof(cname),                           \
                                                  .struct_desc = &sireflect_desc(cname),           \
                                                  __VA_ARGS__ };                                   \
    ecs_component_t ecs_id(cname) = 0

/*
 * Register a component type in a world.
 *
 * Must be called before using the typed helpers for that component with this
 * world. Stores the generated component id in ecs_id(cname).
 */
#define ECS_COMPONENT_REGISTER(world, cname)                                                       \
    ecs_id(cname) = ecs_component_init(world, &ecs_id(cname##_desc))

/*
 * Declare and define a component type
 */
#define ECS_COMPONENT(cname, ...)                                                                  \
    ECS_COMPONENT_DECLARE(cname, __VA_ARGS__);                                                     \
    ECS_COMPONENT_DEFINE(cname);

/*
 * Declare a typed module.
 *
 * Use in a header:
 *   ECS_MODULE_DECLARE(physics, { float gravity; });
 *
 * This declares physics_props_t, the public module id symbol ecs_id(physics),
 * an import wrapper, and the user-defined import function:
 *   void physics_import(ecs_world_t *world, const physics_props_t *props);
 */
#define ECS_MODULE_DECLARE(module_name, ...)                                                       \
    typedef struct module_name##_props_t __VA_ARGS__ module_name##_props_t;                        \
    extern ecs_module_id_t ecs_id(module_name);                                                    \
    void ecs_id(module_name##_import_wrapper)(ecs_world_t *world, const void *desc);                \
    void module_name##_import(ecs_world_t *world, const module_name##_props_t *props)

/*
 * Define a typed module declared with ECS_MODULE_DECLARE.
 *
 * Use once in a C file before implementing module_name_import.
 */
#define ECS_MODULE_DEFINE(module_name)                                                             \
    ecs_module_id_t ecs_id(module_name) = 0;                                                       \
    void ecs_id(module_name##_import_wrapper)(ecs_world_t *world, const void *desc) {               \
        module_name##_import(world, (const module_name##_props_t *)desc);                          \
    }

/*
 * Import a typed module into a world.
 *
 * The first import calls module_name_import and stores the returned module id in
 * ecs_id(module_name). Later imports of the same module in the same world return
 * the existing id without calling module_name_import again; the first props
 * value wins.
 */
#define ECS_MODULE_IMPORT(world, module_name, ...)                                                 \
    (ecs_id(module_name) = ecs_module_init(                                                        \
         world,                                                                                    \
         &(ecs_module_desc_t){                                                                     \
             .name = #module_name,                                                                 \
             .id = &ecs_id(module_name),                                                           \
             .import = ecs_id(module_name##_import_wrapper),                                       \
             .desc = &(module_name##_props_t)__VA_ARGS__,                                          \
             .desc_size = sizeof(module_name##_props_t),                                           \
         }                                                                                         \
     ))

/* Register/import a module with a raw descriptor. Prefer ECS_MODULE_IMPORT for typed modules. */
#define ecs_module(world, ...) ecs_module_init(world, &(ecs_module_desc_t)__VA_ARGS__)

ecs_module_id_t ecs_module_init(ecs_world_t *world, const ecs_module_desc_t *desc);

/*
 * Find an already imported module by its id storage.
 *
 * For typed modules, pass &ecs_id(module_name). Returns 0 if the
 * module has not been imported into this world.
 */
ecs_module_id_t ecs_module_find(ecs_world_t *world, const ecs_module_id_t *id);
void ecs_module_enable(ecs_world_t *world, ecs_module_id_t module);
void ecs_module_disable(ecs_world_t *world, ecs_module_id_t module);
bool ecs_module_is_enabled(const ecs_world_t *world, ecs_module_id_t module);

/*
 * Define a relation component.
 *
 * A relation stores an entity target. The implementation also creates a source
 * component used internally to track reverse links.
 */
#define ECS_RELATION_DEFINE(cname)                                                                 \
    ecs_component_desc_t ecs_id(cname##_desc) = {                                                  \
        .name = #cname,                                                                            \
        .size = sizeof(cname),                                                                     \
        .is_relation = true,                                                                       \
    };                                                                                             \
    ecs_component_t ecs_id(cname) = 0

/* Return the internal source component id associated with a relation id. */
#define ecs_source(name) (ecs_id(name) + 1)

/* Declare a relation type. The generated struct contains ecs_entity_t target.
 */
#define ECS_RELATION_DECLARE(name) ECS_COMPONENT_DECLARE(name, { ecs_entity_t target; })

/* Builtin relation for parent/child relationships. */
ECS_RELATION_DECLARE(ChildOf);

/* Builtin component for inheritance relationships. */
ECS_COMPONENT_DECLARE(IsA, { ecs_entity_t target; });

/* Builtin component for entity names. */
ECS_COMPONENT_DECLARE(Name, { char *value; });

/*
 * Register a component from an inline descriptor.
 *
 * Example:
 *   ecs_component_t Position = ecs_component(world, {
 *       .name = "Position",
 *       .size = sizeof(Position)
 *   });
 */
#define ecs_component(world, ...) ecs_component_init(world, &(ecs_component_desc_t)__VA_ARGS__)

/* Register a component descriptor and return its component id. */
ecs_component_t ecs_component_init(ecs_world_t *world, const ecs_component_desc_t *desc);

/* Create a new alive entity in world. world must not be NULL. */
ecs_entity_t ecs_new(ecs_world_t *world);

/*
 * Return whether entity is alive in world.
 *
 * entity must be a handle created by this world. Passing arbitrary ids is not a
 * supported validity check.
 */
int ecs_is_alive(const ecs_world_t *world, ecs_entity_t entity);

/* Destroy an alive entity and remove all of its components. */
void ecs_kill(ecs_world_t *world, ecs_entity_t entity);

/*
 * Create a query from an inline descriptor.
 *
 * Example:
 *   ecs_query_id_t q = ecs_query(world, {
 *       .terms = { ecs_in(Position), ecs_in(Velocity) }
 *   });
 */
#define ecs_query(world, ...) ecs_query_init(world, &(ecs_query_desc_t)__VA_ARGS__)

/* Create a query. The query descriptor must read at least one component. */
uint32_t ecs_query_init(ecs_world_t *world, const ecs_query_desc_t *query);

/* Destroy a query id created by ecs_query/ecs_query_init. */
void ecs_query_fini(ecs_world_t *world, ecs_query_id_t qid);

/* Add a typed component tag/storage to an alive entity. */
#define ecs_add(world, entity, cname) ecs_add_cid(world, entity, ecs_id(cname))

/*
 * Add a component id to an alive entity.
 *
 * If the component is already present, this is currently treated as a no-op by
 * table migration. The component id must be registered in the same world.
 */
void ecs_add_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);

/* Remove a typed component from an alive entity. */
#define ecs_remove(world, entity, cname) ecs_remove_cid(world, entity, ecs_id(cname))

/*
 * Remove a component id from an alive entity.
 *
 * Removing a component that is not present is a no-op.
 */
void ecs_remove_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);

/* Return whether an alive entity has a typed component. */
#define ecs_has(world, entity, cname) ecs_has_cid(world, entity, ecs_id(cname))

/* Return whether an alive entity has a component id. */
bool ecs_has_cid(const ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);

/*
 * Get a typed component pointer from an alive entity.
 *
 * The component is assumed to exist. Use ecs_try_get when the component may be
 * absent.
 */
#define ecs_get(world, entity, cname) ((cname *)ecs_get_cid(world, entity, ecs_id(cname)))

/*
 * Get a component pointer by id.
 *
 * The component is assumed to exist on the entity. Use ecs_try_get_cid when the
 * component may be absent.
 */
void *ecs_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);

/* Get a typed component pointer, or NULL if the entity does not have it. */
#define ecs_try_get(world, entity, cname) ((cname *)ecs_try_get_cid(world, entity, ecs_id(cname)))

/* Get a component pointer by id, or NULL if the entity does not have it. */
void *ecs_try_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid);

/*
 * Set a typed component value on an alive entity.
 *
 * Adds the component if needed, then emits OnSet. Component on_set hooks and
 * OnSet observers receive the new value before it is copied into storage.
 */
#define ecs_set(world, entity, cname, ...)                                                         \
    ecs_set_cid(world, entity, ecs_id(cname), &(cname)__VA_ARGS__)

/*
 * Set a component value by id.
 *
 * data must point to at least the registered component size. Adds the component
 * if needed.
 */
void ecs_set_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id, const void *data);

/*
 * Declare that adding component also adds require first.
 *
 * Requirement cycles are debug assertion failures when declared.
 */
void ecs_with(ecs_world_t *world, ecs_component_t component, ecs_component_t require);

/* Builtin observer events. */
#define OnAdd 0
#define OnRemove 1
#define OnSet 2

/* Observer descriptor. callback is required. */
typedef struct {
    ecs_event_t on;
    ecs_query_desc_t query;
    ecs_observer_callback_t callback;
    uintptr_t user_data;
} ecs_observer_desc_t;

/* Create an observer from an inline descriptor. */
#define ecs_observer(world, ...) ecs_observer_init(world, &(ecs_observer_desc_t)__VA_ARGS__)

/* Allocate and return a custom event id. */
ecs_event_t ecs_event(ecs_world_t *world);

/* Create an observer. desc->callback must not be NULL. */
ecs_observer_id_t ecs_observer_init(ecs_world_t *world, const ecs_observer_desc_t *desc);

void ecs_observer_enable(ecs_world_t *world, ecs_observer_id_t id);
void ecs_observer_disable(ecs_world_t *world, ecs_observer_id_t id);

/*
 * Trigger an event for an alive entity.
 *
 * Observers matching the entity's current table and event id will be called.
 */
void ecs_observer_trigger(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_event_t event,
    const void *trigger_data
);

/*
 * Query iterator.
 *
 * Users may read world and count. The other fields are implementation details
 * and should not be accessed directly.
 */
typedef struct {
    ecs_world_t *world;
    uint32_t count;
    struct ecs_query_cache_s *cache;
    void ***ptrs;
    uint16_t table_idx;
    uint16_t table_count;
} ecs_iter_t;

/* Create a stack iterator for a query id. */
ecs_iter_t ecs_query_iter(ecs_world_t *world, ecs_query_id_t query_id);

/*
 * Advance an iterator to the next non-empty batch.
 *
 * Returns false when iteration is finished. it->count is the number of entities
 * in the current batch.
 */
bool ecs_iter_next(ecs_iter_t *it);

/*
 * Return the component array for a read term in the current iterator batch.
 *
 * field_index is zero-based among EcsIn, EcsOut and EcsInOut terms only.
 * EcsFilter and EcsNot terms affect matching but are not returned as fields.
 */
static inline void *ecs_field(ecs_iter_t *it, uint16_t field_index) {
    return *it->ptrs[field_index];
}

/* System phases run in enum order when ecs_progress is called. */
typedef enum {
    EcsPreStart,
    EcsStart,
    EcsPostStart,
    EcsOnLoad,
    EcsPostLoad,
    EcsPreUpdate,
    EcsOnUpdate,
    EcsPostUpdate,
    EcsPreRender,
    EcsOnRender,
    EcsPostRender,
    EcsPhaseCount,
} ecs_phase_t;

/* Backward-compatible phase aliases. Prefer the Ecs* names in new code. */
#define OnPreUpdate EcsPreUpdate
#define OnUpdate EcsOnUpdate
#define OnPostUpdate EcsPostUpdate
#define OnRender EcsOnRender

/*
 * System descriptor.
 *
 * callback is called once per non-empty iterator batch matching query. phase
 * controls when ecs_progress/ecs_run_phase executes the system. after contains
 * up to four system ids that must run before this system in the same phase.
 */
typedef struct {
    const char *name;
    ecs_query_desc_t query;
    void (*callback)(ecs_iter_t *);
    ecs_phase_t phase;
    ecs_system_id_t after[4];
    bool disabled;
} ecs_system_desc_t;

/* Create a system from an inline descriptor. */
#define ecs_system(world, ...) ecs_system_init(world, &(ecs_system_desc_t)__VA_ARGS__)

/* Register a system and return its id. System id 0 is reserved. */
ecs_system_id_t ecs_system_init(ecs_world_t *world, const ecs_system_desc_t *desc);

/* Run all enabled systems in phase order. */
bool ecs_progress(ecs_world_t *world);

/* Run all enabled systems from one phase. */
void ecs_run_phase(ecs_world_t *world, ecs_phase_t phase);

/* Run one enabled system immediately. */
void ecs_run_system(ecs_world_t *world, ecs_system_id_t system);

/* Enable or disable a system. Disabled systems stay registered but do not run.
 */
void ecs_system_enable(ecs_world_t *world, ecs_system_id_t system);
void ecs_system_disable(ecs_world_t *world, ecs_system_id_t system);

#ifdef __cplusplus
}
#endif

#endif
