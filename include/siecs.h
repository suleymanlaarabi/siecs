#ifndef SIECS_H
#define SIECS_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "siecs/bake_config.h"

#ifdef __cplusplus
#ifndef _Alignof
#define _Alignof alignof
#endif
#endif

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
typedef uint16_t ecs_resource_t;
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
 * - EcsOnAdd: pointer to the added component storage.
 * - EcsOnRemove: pointer to the component storage before removal.
 * - EcsOnSet: pointer to the new value passed to ecs_set/ecs_set_cid.
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

/* Called after a component slot is added and zero-initialized. */
typedef void (*ecs_component_on_add_t)(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t component,
    void *value
);

/*
 * Called before ecs_set_cid copies new_value into current_value.
 *
 * current_value is the component slot currently stored on the entity. Hooks can
 * inspect the old value there, and may mutate it before the final copy.
 */
typedef void (*ecs_component_on_set_t)(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t component,
    const void *new_value,
    void *current_value
);

/* Called before a component slot is removed. */
typedef void (*ecs_component_on_remove_t)(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t component,
    void *value
);

/*
 * Relation behavior flags used by ECS_RELATION_DEFINE.
 *
 * EcsRelationTarget marks the relation component stored on the source entity.
 * EcsRelationSource marks the internal reverse-link component stored on target
 * entities. EcsRelationCascadeDelete kills related source entities when the
 * target entity is killed.
 */
typedef enum {
    EcsRelationTarget = 1 << 0,
    EcsRelationSource = 1 << 1,
    EcsRelationCascadeDelete = 1 << 2,
} ecs_relation_flags_t;

/*
 * Component registration descriptor.
 *
 * name is used for reflection/debug/introspection. size is sizeof(T) for data
 * components and 0 for tags. Hooks are optional lifecycle callbacks.
 * struct_desc enables reflection for ECS_COMPONENT_* generated types.
 *
 * relation_flags is normally set only by ECS_RELATION_DEFINE.
 */
typedef struct {
    const char *name;
    uint64_t size;
    ecs_component_on_set_t on_set;
    ecs_component_on_remove_t on_remove;
    ecs_component_on_add_t on_add;
    uint32_t relation_flags;
    const sireflect_struct_desc_t *struct_desc;
} ecs_component_desc_t;

/*
 * Resource hook function type.
 *
 * ptr points to the resource value being set or removed. The pointer is only
 * valid for the duration of the callback.
 */
typedef void (*ecs_resource_hook_t)(ecs_world_t *world, const void *ptr);

/*
 * Resource registration descriptor.
 *
 * Resources are per-world singleton values with their own id space. name and
 * size are required; hooks are optional.
 */
typedef struct {
    const char *name;
    uint64_t size;
    ecs_resource_hook_t on_set;
    ecs_resource_hook_t on_remove;
} ecs_resource_desc_t;

/* Query term access mode. */
typedef enum {
    EcsIn,            /* Component must exist and is returned by ecs_field for reading. */
    EcsOut,           /* Component must exist and is returned by ecs_field for writing. */
    EcsInOut,         /* Component must exist and is returned by ecs_field for read/write. */
    EcsInOptional,    /* Component is returned by ecs_field if present, NULL otherwise. */
    EcsInOutOptional, /* Component is returned by ecs_field if present, NULL otherwise. */
    EcsFilter,        /* Component must exist but is not returned by ecs_field. */
    EcsNot,           /* Component must not exist and is not returned by ecs_field. */
} ecs_term_access_t;

/*
 * Query term descriptor.
 *
 * Prefer ecs_in/ecs_out/ecs_inout/ecs_filter/ecs_not helpers in user code.
 * Fill this manually only for dynamic component ids.
 */
typedef struct {
    ecs_component_t id;
    ecs_term_access_t access;
} ecs_query_term_t;

/*
 * Query descriptor.
 *
 * terms is a zero-terminated component term list. Terms with EcsIn, EcsOut,
 * EcsInOut, EcsInOptional and EcsInOutOptional are returned by ecs_field in
 * declaration order. Optional fields return NULL for tables without the
 * component. EcsFilter and EcsNot only affect table matching.
 *
 * A query must contain at least one term.
 */
typedef struct {
    ecs_query_term_t terms[16];
} ecs_query_desc_t;

/*
 * Typed query term helpers.
 *
 * Example:
 *   ecs_query_id_t q = ecs_query(world, {
 *       .terms = { ecs_inout(Position), ecs_in(Velocity), ecs_filter(Player) }
 *   });
 */
#ifdef __cplusplus
#define ecs_in(cname)                                                                              \
    ecs_query_term_t { ecs_id(cname), EcsIn }
#define ecs_out(cname)                                                                             \
    ecs_query_term_t { ecs_id(cname), EcsOut }
#define ecs_inout(cname)                                                                           \
    ecs_query_term_t { ecs_id(cname), EcsInOut }
#define ecs_in_optional(cname)                                                                     \
    ecs_query_term_t { ecs_id(cname), EcsInOptional }
#define ecs_inout_optional(cname)                                                                  \
    ecs_query_term_t { ecs_id(cname), EcsInOutOptional }
#define ecs_filter(cname)                                                                          \
    ecs_query_term_t { ecs_id(cname), EcsFilter }
#define ecs_not(cname)                                                                             \
    ecs_query_term_t { ecs_id(cname), EcsNot }
#else
#define ecs_in(cname) ((ecs_query_term_t){ ecs_id(cname), EcsIn })
#define ecs_out(cname) ((ecs_query_term_t){ ecs_id(cname), EcsOut })
#define ecs_inout(cname) ((ecs_query_term_t){ ecs_id(cname), EcsInOut })
#define ecs_in_optional(cname) ((ecs_query_term_t){ ecs_id(cname), EcsInOptional })
#define ecs_inout_optional(cname) ((ecs_query_term_t){ ecs_id(cname), EcsInOutOptional })
#define ecs_filter(cname) ((ecs_query_term_t){ ecs_id(cname), EcsFilter })
#define ecs_not(cname) ((ecs_query_term_t){ ecs_id(cname), EcsNot })
#endif

/* Create an ECS world. */
SIECS_API ecs_world_t *ecs_init(void);

/* World feature descriptor. */
typedef struct {
    /* Start the REST explorer server when the rest addon is built in. */
    bool rest;
} ecs_world_feat_desc_t;

/* Create a world with the given features. */
#define ecs_with_features(...) ecs_init_w_features(&(ecs_world_feat_desc_t)__VA_ARGS__)

/* Initialize a world with the given features. */
SIECS_API ecs_world_t *ecs_init_w_features(const ecs_world_feat_desc_t *features);

/* Destroy a world and all ECS storage owned by it. world must not be NULL. */
SIECS_API void ecs_fini(ecs_world_t *world);

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
    ecs_id(cname) = ecs_component_register(world, &ecs_id(cname), &ecs_id(cname##_desc))

/*
 * Declare and define a component type in one translation unit.
 *
 * Example:
 *   ECS_COMPONENT(Position, { float x; float y; });
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
    void ecs_id(module_name##_import_wrapper)(ecs_world_t * world, const void *desc);              \
    void module_name##_import(ecs_world_t *world, const module_name##_props_t *props)

/*
 * Define a typed module declared with ECS_MODULE_DECLARE.
 *
 * Use once in a C file before implementing module_name_import.
 */
#define ECS_MODULE_DEFINE(module_name)                                                             \
    ecs_module_id_t ecs_id(module_name) = 0;                                                       \
    void ecs_id(module_name##_import_wrapper)(ecs_world_t * world, const void *desc) {             \
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

/*
 * Register/import a module with a raw descriptor.
 *
 * Prefer ECS_MODULE_IMPORT for typed modules. Use this when module properties
 * or ids are built dynamically.
 */
#define ecs_module(world, ...) ecs_module_init(world, &(ecs_module_desc_t)__VA_ARGS__)

/* Register/import a module descriptor and return its module id. */
SIECS_API ecs_module_id_t ecs_module_init(ecs_world_t *world, const ecs_module_desc_t *desc);

/*
 * Find an already imported module by its id storage.
 *
 * For typed modules, pass &ecs_id(module_name). Returns 0 if the
 * module has not been imported into this world.
 */
SIECS_API ecs_module_id_t ecs_module_find(ecs_world_t *world, const ecs_module_id_t *id);

/* Enable systems and observers recorded during module import. */
SIECS_API void ecs_module_enable(ecs_world_t *world, ecs_module_id_t module);

/* Disable systems and observers recorded during module import. */
SIECS_API void ecs_module_disable(ecs_world_t *world, ecs_module_id_t module);

/* Return whether a module is currently enabled in this world. */
SIECS_API bool ecs_module_is_enabled(const ecs_world_t *world, ecs_module_id_t module);

/*
 * Define a relation component.
 *
 * A relation stores an entity target. The implementation also creates a source
 * component used internally to track reverse links.
 *
 * Example:
 *   ECS_RELATION_DECLARE(ParentOf);
 *   ECS_RELATION_DEFINE(ParentOf, EcsRelationCascadeDelete);
 */
#define ECS_RELATION_DEFINE(cname, flags)                                                          \
    ecs_component_desc_t ecs_id(cname##_desc) = {                                                  \
        .name = #cname,                                                                            \
        .size = sizeof(cname),                                                                     \
        .relation_flags = EcsRelationTarget | (flags),                                             \
    };                                                                                             \
    ecs_component_t ecs_id(cname) = 0

/* Return the internal source component id associated with a relation id. */
#define ecs_source(name) (ecs_id(name) + 1)

/*
 * Declare a relation type. The generated struct contains ecs_entity_t target.
 *
 * Example:
 *   ECS_RELATION_DECLARE(Targets);
 *   ecs_set(world, entity, Targets, { target });
 */
#define ECS_RELATION_DECLARE(name) ECS_COMPONENT_DECLARE(name, { ecs_entity_t target; })

/* Builtin relation for parent/child relationships. */
ECS_RELATION_DECLARE(ChildOf);

/* Builtin component for entity names. */
ECS_COMPONENT_DECLARE(Name, { char *value; });

/* Builtin component excluded from queries by default. */
ECS_COMPONENT_DECLARE(Disabled, {});

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
SIECS_API ecs_component_t ecs_component_init(ecs_world_t *world, const ecs_component_desc_t *desc);

/* Register a typed component descriptor using stable process-wide id storage. */
SIECS_API ecs_component_t
ecs_component_register(ecs_world_t *world, ecs_component_t *id, const ecs_component_desc_t *desc);

/* Create a new alive entity in world. world must not be NULL. */
SIECS_API ecs_entity_t ecs_new(ecs_world_t *world);

/*
 * Return whether entity is alive in world.
 *
 * entity must be a handle created by this world. Passing arbitrary ids is not a
 * supported validity check.
 */
SIECS_API bool ecs_is_alive(const ecs_world_t *world, ecs_entity_t entity);

SIECS_API void ecs_is_a(ecs_world_t *world, ecs_entity_t entity, ecs_entity_t target);

/* Destroy an alive entity and remove all of its components. */
SIECS_API void ecs_kill(ecs_world_t *world, ecs_entity_t entity);

/*
 * Create a query from an inline descriptor.
 *
 * Example:
 *   ecs_query_id_t q = ecs_query(world, {
 *       .terms = { ecs_in(Position), ecs_in(Velocity) }
 *   });
 */
#define ecs_query(world, ...) ecs_query_init(world, &(ecs_query_desc_t)__VA_ARGS__)

/*
 * Convenience query loop for short-lived queries.
 *
 * This macro creates a temporary query, iterates it, then destroys it. Prefer a
 * persistent ecs_query_id_t for systems, hot paths, or repeated frame work.
 *
 * Example:
 *   ecs_query_each(world, it, i, ecs_in(Position), ecs_in(Velocity)) {
 *       Position *p = ecs_field(&it, 0);
 *       Velocity *v = ecs_field(&it, 1);
 *       p[i].x += v[i].x;
 *   }
 */
#define ecs_query_each(world, it, i, ...)                                                          \
    for (ecs_query_id_t _q = ecs_query((world), { { __VA_ARGS__ } }); _q;                          \
         ecs_query_fini((world), _q), _q = 0)                                                      \
        for (ecs_iter_t it = ecs_query_iter((world), _q); ecs_iter_next(&it);)                     \
            for (uint32_t i = 0; i < it.count; i++)

/* Create a query. The query descriptor must read at least one component. */
SIECS_API uint32_t ecs_query_init(ecs_world_t *world, const ecs_query_desc_t *query);

/* Destroy a query id created by ecs_query/ecs_query_init. */
SIECS_API void ecs_query_fini(ecs_world_t *world, ecs_query_id_t qid);

/* Add a typed component tag/storage to an alive entity. */
#define ecs_add(world, entity, cname) ecs_add_cid(world, entity, ecs_id(cname))

/*
 * Add a component id to an alive entity.
 *
 * If the component is already present, this is currently treated as a no-op by
 * table migration. The component id must be registered in the same world.
 */
SIECS_API void ecs_add_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);

/* Remove a typed component from an alive entity. */
#define ecs_remove(world, entity, cname) ecs_remove_cid(world, entity, ecs_id(cname))

/*
 * Remove a component id from an alive entity.
 *
 * Removing a component that is not present is a no-op.
 */
SIECS_API void ecs_remove_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);

/* Return whether an alive entity has a typed component. */
#define ecs_has(world, entity, cname) ecs_has_cid(world, entity, ecs_id(cname))

/* Return whether an alive entity has a component id. */
SIECS_API bool ecs_has_cid(const ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);

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
SIECS_API void *ecs_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id);

/* Get a typed component pointer, or NULL if the entity does not have it. */
#define ecs_try_get(world, entity, cname) ((cname *)ecs_try_get_cid(world, entity, ecs_id(cname)))

/* Get a component pointer by id, or NULL if the entity does not have it. */
SIECS_API void *ecs_try_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid);

/*
 * Set a typed component value on an alive entity.
 *
 * Adds the component if needed, then emits EcsOnSet. Component on_set hooks receive
 * the new value and current storage before the copy. EcsOnSet observers receive the
 * new value before it is copied into storage.
 */
#define ecs_set(world, entity, cname, ...)                                                         \
    ecs_set_cid(world, entity, ecs_id(cname), &(cname)__VA_ARGS__)

/*
 * Set a component value by id.
 *
 * data must point to at least the registered component size. Adds the component
 * if needed.
 */
SIECS_API void
ecs_set_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id, const void *data);

/*
 * Declare and define a resource type.
 *
 * Resources use their own id space and are stored once per world. They are not
 * components, do not appear in queries, and do not consume component ids.
 *
 * Use ECS_RESOURCE_DECLARE in headers and ECS_RESOURCE_DEFINE once in a C file,
 * or ECS_RESOURCE for local examples/tests.
 *
 * Example:
 *   ECS_RESOURCE(Time, { float dt; float elapsed; });
 */
#define ECS_RESOURCE_DECLARE(rname, ...)                                                           \
    typedef struct rname rname;                                                                    \
    struct rname __VA_ARGS__;                                                                      \
    extern ecs_resource_t ecs_id(rname);                                                           \
    extern ecs_resource_desc_t ecs_id(rname##_desc)

#define ECS_RESOURCE_DEFINE(rname, ...)                                                            \
    ecs_resource_desc_t ecs_id(rname##_desc) = { .name = #rname,                                   \
                                                 .size = sizeof(rname),                            \
                                                 __VA_ARGS__ };                                    \
    ecs_resource_t ecs_id(rname) = 0

#define ECS_RESOURCE_REGISTER(world, rname)                                                        \
    ecs_id(rname) = ecs_resource_init(world, &ecs_id(rname##_desc))

#define ECS_RESOURCE(rname, ...)                                                                   \
    ECS_RESOURCE_DECLARE(rname, __VA_ARGS__);                                                      \
    ECS_RESOURCE_DEFINE(rname)

/*
 * Set or replace a world resource.
 *
 * Example:
 *   ecs_set_resource(world, Time, { .dt = 0.016f, .elapsed = 0.0f });
 */
#define ecs_set_resource(world, rname, ...)                                                        \
    ecs_set_resource_rid(world, ecs_id(rname), &(rname)__VA_ARGS__)

/* Get a world resource. The resource must exist. */
#define ecs_get_resource(world, rname) ((rname *)ecs_resource_rid(world, ecs_id(rname)))
#define ecs_get_resource_read(world, rname) ((const rname *)ecs_resource_rid(world, ecs_id(rname)))

/* Get a world resource, or NULL if it does not exist. */
#define ecs_try_get_resource(world, rname) ((rname *)ecs_try_resource_rid(world, ecs_id(rname)))
#define ecs_try_get_resource_read(world, rname)                                                    \
    ((const rname *)ecs_try_resource_rid(world, ecs_id(rname)))

/* Return whether a world resource exists. */
#define ecs_has_resource(world, rname) ecs_has_resource_rid(world, ecs_id(rname))

/* Remove a world resource if it exists. */
#define ecs_remove_resource(world, rname) ecs_remove_resource_rid(world, ecs_id(rname))

/* Backward-compatible resource aliases. Prefer the ecs_get_resource names in new code. */
#define ecs_resource(world, rname) ecs_get_resource(world, rname)
#define ecs_resource_read(world, rname) ecs_get_resource_read(world, rname)
#define ecs_try_resource(world, rname) ecs_try_get_resource(world, rname)
#define ecs_try_resource_read(world, rname) ecs_try_get_resource_read(world, rname)

SIECS_API ecs_resource_t ecs_resource_init(ecs_world_t *world, const ecs_resource_desc_t *desc);
SIECS_API ecs_resource_t ecs_resource_find(ecs_world_t *world, const char *name);
SIECS_API bool ecs_resource_is_registered_rid(const ecs_world_t *world, ecs_resource_t id);
SIECS_API void ecs_set_resource_rid(ecs_world_t *world, ecs_resource_t id, const void *data);
SIECS_API void *ecs_resource_rid(ecs_world_t *world, ecs_resource_t id);
SIECS_API void *ecs_try_resource_rid(ecs_world_t *world, ecs_resource_t id);
SIECS_API bool ecs_has_resource_rid(const ecs_world_t *world, ecs_resource_t id);
SIECS_API void ecs_remove_resource_rid(ecs_world_t *world, ecs_resource_t id);

/*
 * Declare that adding component also adds require first.
 *
 * Requirement cycles are debug assertion failures when declared.
 *
 * Example:
 *   ecs_with(world, ecs_id(Renderable), ecs_id(Transform));
 *   ecs_add(world, entity, Renderable); // also adds Transform
 */
SIECS_API void ecs_with(ecs_world_t *world, ecs_component_t component, ecs_component_t require);

/* Builtin observer events. */
#define EcsOnAdd 0
#define EcsOnRemove 1
#define EcsOnSet 2

/*
 * Observer descriptor.
 *
 * callback is required. query describes which entities can receive the event.
 * user_data is copied into ecs_observer_event_t for the callback.
 */
typedef struct {
    ecs_event_t on;
    ecs_query_desc_t query;
    ecs_observer_callback_t callback;
    uintptr_t user_data;
} ecs_observer_desc_t;

/*
 * Create an observer from an inline descriptor.
 *
 * Example:
 *   ecs_observer(world, {
 *       .on = EcsOnSet,
 *       .query.terms = { ecs_in(Position) },
 *       .callback = on_position_set,
 *   });
 */
#define ecs_observer(world, ...) ecs_observer_init(world, &(ecs_observer_desc_t)__VA_ARGS__)

/* Allocate and return a custom event id for ecs_observer_trigger. */
SIECS_API ecs_event_t ecs_event(ecs_world_t *world);

/* Create an observer. desc->callback must not be NULL. */
SIECS_API ecs_observer_id_t ecs_observer_init(ecs_world_t *world, const ecs_observer_desc_t *desc);

SIECS_API void ecs_observer_enable(ecs_world_t *world, ecs_observer_id_t id);
SIECS_API void ecs_observer_disable(ecs_world_t *world, ecs_observer_id_t id);

/*
 * Trigger an event for an alive entity.
 *
 * Observers matching the entity's current table and event id will be called.
 */
SIECS_API void ecs_observer_trigger(
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
 *
 * entities points to the current batch after ecs_iter_next returns true.
 */
typedef struct {
    ecs_world_t *world;
    uint32_t count;
    ecs_entity_t *entities;
    struct ecs_query_cache_s *cache;
    void ***ptrs;
    uint16_t table_idx;
    uint16_t table_count;
} ecs_iter_t;

/*
 * Create a stack iterator for a query id.
 *
 * Example:
 *   ecs_iter_t it = ecs_query_iter(world, query);
 *   while (ecs_iter_next(&it)) {
 *       Position *p = ecs_field(&it, 0);
 *   }
 */
SIECS_API ecs_iter_t ecs_query_iter(ecs_world_t *world, ecs_query_id_t query_id);

/*
 * Advance an iterator to the next non-empty batch.
 *
 * Returns false when iteration is finished. it->count is the number of entities
 * in the current batch.
 */
SIECS_API bool ecs_iter_next(ecs_iter_t *it);

/*
 * Return the component array for a read term in the current iterator batch.
 *
 * field_index is zero-based among EcsIn, EcsOut, EcsInOut, EcsInOptional and
 * EcsInOutOptional terms only. Optional fields return NULL when the current
 * table does not have the component. EcsFilter and EcsNot terms affect matching
 * but are not returned as fields.
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
 * If query has terms, callback is called once per non-empty iterator batch
 * matching query. If query has no terms, the system runs once with an empty
 * iterator. phase controls when ecs_progress/ecs_run_phase executes the system.
 * after contains up to four system ids that must run before this system in the
 * same phase.
 */
typedef struct {
    const char *name;
    ecs_query_desc_t query;
    void (*callback)(ecs_iter_t *);
    ecs_phase_t phase;
    ecs_system_id_t after[4];
    bool disabled;
} ecs_system_desc_t;

/*
 * Create a system from an inline descriptor.
 *
 * Example:
 *   ecs_system(world, {
 *       .name = "Move",
 *       .phase = EcsOnUpdate,
 *       .query.terms = { ecs_inout(Position), ecs_in(Velocity) },
 *       .callback = Move,
 *   });
 */
#define ecs_system(world, ...) ecs_system_init(world, &(ecs_system_desc_t)__VA_ARGS__)

/* Register a system and return its id. System id 0 is reserved. */
SIECS_API ecs_system_id_t ecs_system_init(ecs_world_t *world, const ecs_system_desc_t *desc);

/* Run all enabled systems in phase order. */
SIECS_API bool ecs_progress(ecs_world_t *world);

/* Run all enabled systems from one phase. */
SIECS_API void ecs_run_phase(ecs_world_t *world, ecs_phase_t phase);

/* Run one enabled system immediately. */
SIECS_API void ecs_run_system(ecs_world_t *world, ecs_system_id_t system);

/* Enable or disable a system. Disabled systems stay registered but do not run.
 */
SIECS_API void ecs_system_enable(ecs_world_t *world, ecs_system_id_t system);
SIECS_API void ecs_system_disable(ecs_world_t *world, ecs_system_id_t system);

#ifdef __cplusplus
}
#endif

#endif
