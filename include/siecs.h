#ifndef SIECS_H
#define SIECS_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "siecs/config.h"

#if SICORE_VEC
#include <sicore.h>
#endif

#ifndef SIJSON_H
#include <sijson.h>
#endif
#ifndef SIREFLECT_H
#include <sireflect.h>
#endif

#ifndef siecs_STATIC
#if defined(siecs_EXPORTS) && (defined(_MSC_VER) || defined(__MINGW32__))
#define SIECS_API __declspec(dllexport)
#elif defined(siecs_EXPORTS)
#define SIECS_API __attribute__((__visibility__("default")))
#elif defined(_MSC_VER)
#define SIECS_API __declspec(dllimport)
#else
#define SIECS_API
#endif
#else
#define SIECS_API
#endif

#ifdef __cplusplus
#ifndef _Alignof
#define _Alignof alignof
#endif

namespace ecs {
namespace detail {

template <typename T> struct c_component_traits {
  static constexpr bool value = false;
};

template <typename T> struct c_resource_traits {
  static constexpr bool value = false;
};

template <typename T> struct c_relation_traits {
  static constexpr bool value = false;
};

} // namespace detail
} // namespace ecs
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Public id symbol generated for a component type.
 *
 * Users normally do not call this macro directly. It is used by the typed
 * helpers such as ecs_add(entity, Position).
 */
#define ecs_id(name) _ecs_id_##name##__
#define ecs_rid(name) _ecs_rid_##name##__

/* Public handle types. A zero id is reserved internally and is not user data.
 */
typedef uint64_t ecs_entity_t;
/* Component id in the active world's component namespace. */
typedef uint16_t ecs_component_t;
/* Relation id in the active world's separate relation namespace. */
typedef uint16_t ecs_relation_id_t;
/* Opaque query id returned by ecs_query_init. */
typedef uint16_t ecs_query_id_t;
/* Opaque system id returned by ecs_system_init. */
typedef uint16_t ecs_system_id_t;
/* Observer event id; builtin values are EcsOnAdd/EcsOnRemove/EcsOnSet. */
typedef uint16_t ecs_event_t;
/* Opaque module id returned by ecs_module_init. */
typedef uint16_t ecs_module_id_t;
/* Resource id in the separate resource namespace. */
typedef uint16_t ecs_resource_t;
/* Observer id returned by ecs_observer_init. */
typedef uint32_t ecs_observer_id_t;
/* Opaque archetype table passed to query order callbacks. */
typedef struct ecs_table_s ecs_table_t;

/* Extract the index and generation fields from an entity handle. */
#define ecs_entity_id(entity) ((uint32_t)((entity) >> 32))
#define ecs_entity_generation(entity) ((uint32_t)((entity) & 0xffffffffu))

/* Maximum number of terms accepted by a query descriptor. */
#define ECS_QUERY_TERM_CAPACITY 16
#define ECS_QUERY_RELATION_CAPACITY 8
/* Maximum number of explicit same-phase system dependencies. */
#define ECS_SYSTEM_AFTER_CAPACITY 16

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
typedef void (*ecs_module_import_t)(const void *desc);

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
 * disabled imports the module and immediately disables the systems and
 * observers captured during import. Components remain registered.
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
 * - EcsOnRelationSet/EcsOnRelationRemove: pointer to ecs_relation_event_t.
 * - Custom events: pointer passed to ecs_observer_trigger.
 */
typedef struct {
  ecs_entity_t entity;
  ecs_event_t event;
  uintptr_t user_data;
  const void *trigger_data;
} ecs_observer_event_t;

/*
 * Relation transition payload passed to EcsOnRelationSet and
 * EcsOnRelationRemove observers.
 *
 * For a relation add, old_target is 0. For a retarget, both targets are live
 * handles. For a removal, new_target is 0 and old_target is the previous
 * target. The payload is borrowed and valid only during the callback.
 */
typedef struct {
  ecs_relation_id_t relation;
  ecs_entity_t old_target;
  ecs_entity_t new_target;
} ecs_relation_event_t;

/* Observer callback; event storage is valid only during the callback. */
typedef void (*ecs_observer_callback_t)(ecs_observer_event_t *event);

/* Called after a component slot is added and zero-initialized. */
typedef void (*ecs_component_on_add_t)(ecs_entity_t entity,
                                       ecs_component_t component, void *value);

/*
 * Called before ecs_set_cid copies new_value into current_value.
 *
 * current_value is the component slot currently stored on the entity. Hooks can
 * inspect the old value there, and may mutate it before the final copy.
 */
typedef void (*ecs_component_on_set_t)(ecs_entity_t entity,
                                       ecs_component_t component,
                                       const void *new_value,
                                       void *current_value);

/* Called before a component slot is removed. */
typedef void (*ecs_component_on_remove_t)(ecs_entity_t entity,
                                          ecs_component_t component,
                                          void *value);

/*
 * Value lifecycle operations for non-trivial component/resource storage.
 *
 * ctor initializes count values at dst. dtor destroys count live values at ptr.
 * copy_ctor initializes dst from live src values. copy replaces live dst values
 * from live src values. move_ctor initializes dst by consuming live src values.
 * move replaces live dst values by consuming live src values.
 *
 * Any NULL operation falls back to plain C storage behavior: zero initialize
 * for ctor, no-op for dtor, and memcpy for copy/move operations.
 */
typedef void (*ecs_type_ctor_t)(void *dst, uint32_t count);
/* Destroy count live values at ptr. */
typedef void (*ecs_type_dtor_t)(void *ptr, uint32_t count);
/* Copy-construct or assign count values from src to dst. */
typedef void (*ecs_type_copy_t)(void *dst, const void *src, uint32_t count);
/* Move-construct or assign count values, consuming src. */
typedef void (*ecs_type_move_t)(void *dst, void *src, uint32_t count);

/* Iterator storage returned by ecs_query_iter; ptrs/entities are batch views.
 */
typedef struct {
  ecs_type_ctor_t ctor;
  ecs_type_dtor_t dtor;
  ecs_type_copy_t copy_ctor;
  ecs_type_copy_t copy;
  ecs_type_move_t move_ctor;
  ecs_type_move_t move;
} ecs_type_ops_t;

/*
 * Component registration descriptor.
 *
 * name is used for reflection/debug/introspection. size is sizeof(T) for data
 * components and 0 for tags. Hooks are optional lifecycle callbacks.
 * struct_desc enables reflection for ECS_COMPONENT_* generated types.
 *
 */
typedef struct {
  const char *name;
  uint64_t size;
  ecs_type_ops_t ops;
  ecs_component_on_set_t on_set;
  ecs_component_on_remove_t on_remove;
  ecs_component_on_add_t on_add;
  const sireflect_struct_desc_t *struct_desc;
} ecs_component_desc_t;

/* Immutable metadata for a registered component. */
typedef struct {
  const char *name;
  uint64_t size;
  sireflect_handle_t type;
  /* Copied reflection descriptor, borrowed until ecs_fini(). */
  const sireflect_struct_desc_t *reflection;
} ecs_component_info_t;

/* Dynamic reflected component descriptor. Sireflect derives size and alignment.
 */
typedef struct {
  const char *name;
  const char *fields;
} ecs_dynamic_component_desc_t;

/*
 * Resource hook function type.
 *
 * ptr points to the resource value being set or removed. The pointer is only
 * valid for the duration of the callback.
 */
typedef void (*ecs_resource_hook_t)(const void *ptr);

/*
 * Resource registration descriptor.
 *
 * Resources are per-world singleton values with their own id space. name and
 * size are required; hooks are optional.
 */
typedef struct {
  const char *name;
  uint64_t size;
  ecs_type_ops_t ops;
  ecs_resource_hook_t on_set;
  ecs_resource_hook_t on_remove;
} ecs_resource_desc_t;

/* Query term access mode. */
typedef enum {
  EcsIn,    /* Component must exist and is returned by ecs_field for reading. */
  EcsOut,   /* Component must exist and is returned by ecs_field for writing. */
  EcsInOut, /* Component must exist and is returned by ecs_field for read/write.
             */
  EcsInOptional,    /* Component is returned by ecs_field if present, NULL
                       otherwise. */
  EcsInOutOptional, /* Component is returned by ecs_field if present, NULL
                       otherwise. */
  EcsFilter,        /* Component must exist but is not returned by ecs_field. */
  EcsNot,  /* Component must not exist and is not returned by ecs_field. */
  EcsInUp, /* Read the nearest inherited field through an acyclic ByTarget
              relation. */
  EcsInUpOptional, /* Same as EcsInUp, but permits a missing field. */
} ecs_term_access_t;

/*
 * Query term descriptor.
 *
 * Prefer ecs_in/ecs_out/ecs_inout/ecs_filter/ecs_not helpers in user code.
 * Fill this manually only for dynamic component ids.
 */
typedef struct {
  ecs_component_t id;
  uint32_t access;
} ecs_query_term_t;

#define ECS_QUERY_UP_ACCESS(access, relation)                                  \
  ((uint32_t)(access) | ((uint32_t)(relation) << 8))

typedef enum {
  EcsRelationRequired,
  EcsRelationOptional,
  EcsRelationExcluded,
  EcsRelationTarget,
  EcsRelationDepth
} ecs_query_relation_kind_t;

typedef struct {
  ecs_entity_t target;
  ecs_relation_id_t id;
  ecs_query_relation_kind_t kind;
} ecs_query_relation_term_t;

/* Return a negative value when a precedes b, zero for equal order, or a
 * positive value when b precedes a. The callback must not mutate the world. */
typedef int (*ecs_query_order_func_t)(const ecs_table_t *a,
                                      const ecs_table_t *b,
                                      uint64_t data);

typedef struct {
  ecs_query_order_func_t func;
  uint64_t data;
} ecs_query_order_t;

/*
 * Query descriptor.
 *
 * terms is a zero-terminated component term list. Terms with EcsIn, EcsOut,
 * EcsInOut, EcsInOptional and EcsInOutOptional are returned by ecs_field in
 * declaration order. Optional fields return NULL for tables without the
 * component. EcsFilter and EcsNot only affect table matching.
 *
 * A query may contain component terms, relation terms, a table order or an
 * is_a target.
 */
typedef struct {
  ecs_query_term_t terms[ECS_QUERY_TERM_CAPACITY];
  ecs_query_relation_term_t relations[ECS_QUERY_RELATION_CAPACITY];
  ecs_query_order_t order_by;
  ecs_entity_t is_a;
} ecs_query_desc_t;

/*
 * Typed query term helpers.
 *
 * Example:
 *   ecs_query_id_t q = ecs_query({
 *       .terms = { ecs_inout(Position), ecs_in(Velocity), ecs_filter(Player) }
 *   });
 */
#ifdef __cplusplus
/* Match a required component and expose it as a read-only field. */
#define ecs_in(cname)                                                          \
  ecs_query_term_t { ecs_id(cname), EcsIn }
/* Match a required component and expose it as a writable field. */
#define ecs_out(cname)                                                         \
  ecs_query_term_t { ecs_id(cname), EcsOut }
/* Match a required component and expose it as a read/write field. */
#define ecs_inout(cname)                                                       \
  ecs_query_term_t { ecs_id(cname), EcsInOut }
/* Match an optional component as a read-only field, or NULL when absent. */
#define ecs_in_optional(cname)                                                 \
  ecs_query_term_t { ecs_id(cname), EcsInOptional }
/* Match an optional component as a read/write field, or NULL when absent. */
#define ecs_inout_optional(cname)                                              \
  ecs_query_term_t { ecs_id(cname), EcsInOutOptional }
/* Match a component without exposing a field pointer. */
#define ecs_filter(cname)                                                      \
  ecs_query_term_t { ecs_id(cname), EcsFilter }
/* Exclude entities that contain the component. */
#define ecs_not(cname)                                                         \
  ecs_query_term_t { ecs_id(cname), EcsNot }
#define ecs_up(cname, relation)                                                \
  ecs_query_term_t {                                                           \
    ecs_id(cname), ECS_QUERY_UP_ACCESS(EcsInUp, ecs_rid(relation))             \
  }
#define ecs_up_optional(cname, relation)                                       \
  ecs_query_term_t {                                                           \
    ecs_id(cname), ECS_QUERY_UP_ACCESS(EcsInUpOptional, ecs_rid(relation))     \
  }
#else
/* C spellings of the typed query term helpers. */
#define ecs_in(cname) ((ecs_query_term_t){ecs_id(cname), EcsIn})
/* Writable required field. */
#define ecs_out(cname) ((ecs_query_term_t){ecs_id(cname), EcsOut})
/* Read/write required field. */
#define ecs_inout(cname) ((ecs_query_term_t){ecs_id(cname), EcsInOut})
/* Optional read-only field. */
#define ecs_in_optional(cname)                                                 \
  ((ecs_query_term_t){ecs_id(cname), EcsInOptional})
/* Deprecated compatibility alias for ecs_in_optional; use ecs_in_optional. */
#define ecs_optional ecs_in_optional
/* Optional read/write field. */
#define ecs_inout_optional(cname)                                              \
  ((ecs_query_term_t){ecs_id(cname), EcsInOutOptional})
/* Filter-only required component. */
#define ecs_filter(cname) ((ecs_query_term_t){ecs_id(cname), EcsFilter})
/* Excluded component. */
#define ecs_not(cname) ((ecs_query_term_t){ecs_id(cname), EcsNot})
#define ecs_up(cname, relation)                                                \
  ((ecs_query_term_t){ecs_id(cname),                                           \
                      ECS_QUERY_UP_ACCESS(EcsInUp, ecs_rid(relation))})
#define ecs_up_optional(cname, relation)                                       \
  ((ecs_query_term_t){                                                         \
      ecs_id(cname), ECS_QUERY_UP_ACCESS(EcsInUpOptional, ecs_rid(relation))})
#endif

#ifdef __cplusplus
#define ecs_rel(name)                                                          \
  ecs_query_relation_term_t { 0, ecs_rid(name), EcsRelationRequired }
#define ecs_rel_opt(name)                                                      \
  ecs_query_relation_term_t { 0, ecs_rid(name), EcsRelationOptional }
#define ecs_not_rel(name)                                                      \
  ecs_query_relation_term_t { 0, ecs_rid(name), EcsRelationExcluded }
#define ecs_to(name, entity)                                                   \
  ecs_query_relation_term_t { entity, ecs_rid(name), EcsRelationTarget }
#define ecs_depth(name, value)                                                 \
  ecs_query_relation_term_t {                                                  \
    (ecs_entity_t)(value), ecs_rid(name), EcsRelationDepth                     \
  }
#define ecs_order_by_target(name) ecs_order_by_target_id(ecs_rid(name))
#define ecs_order_by_depth(name) ecs_order_by_depth_id(ecs_rid(name))
#else
#define ecs_rel(name)                                                          \
  ((ecs_query_relation_term_t){0, ecs_rid(name), EcsRelationRequired})
#define ecs_rel_opt(name)                                                      \
  ((ecs_query_relation_term_t){0, ecs_rid(name), EcsRelationOptional})
#define ecs_not_rel(name)                                                      \
  ((ecs_query_relation_term_t){0, ecs_rid(name), EcsRelationExcluded})
#define ecs_to(name, entity)                                                   \
  ((ecs_query_relation_term_t){entity, ecs_rid(name), EcsRelationTarget})
#define ecs_depth(name, value)                                                 \
  ((ecs_query_relation_term_t){(ecs_entity_t)(value), ecs_rid(name),           \
                               EcsRelationDepth})
#define ecs_order_by_target(name) ecs_order_by_target_id(ecs_rid(name))
#define ecs_order_by_depth(name) ecs_order_by_depth_id(ecs_rid(name))
#endif

/* Create an ECS world. */
SIECS_API void ecs_init(void);

/* World feature descriptor. */
typedef struct {
  /* Target frames per second for the world's update loop. */
  uint16_t target_fps;
} ecs_world_feat_desc_t;

/* Create a world with the given features. */
#define ecs_with_features(...)                                                 \
  ecs_init_w_features(&(ecs_world_feat_desc_t)__VA_ARGS__)

/* Initialize a world with the given features. */
SIECS_API void ecs_init_w_features(const ecs_world_feat_desc_t *features);

/* Destroy a world and all ECS storage owned by it. world must not be NULL. */
SIECS_API void ecs_fini(void);

/* Request that future ecs_progress calls return false. */
SIECS_API void ecs_quit(void);

/*
 * Declare a component type and its public component id.
 *
 * Use in headers:
 *   ECS_COMPONENT_DECLARE(Position, { float x; float y; });
 *
 * In C++, the generated type can be passed directly to ecs::component<T>()
 * and the typed entity/query helpers. Those helpers reuse the C id and
 * descriptor instead of creating a second registration.
 */
/* Declare a component type and its descriptor in a public header. */
#define ECS_COMPONENT_DECLARE(cname, ...)                                      \
  SIJSON_DECLARE(cname, __VA_ARGS__)                                           \
  extern ecs_component_t ecs_id(cname);                                        \
  extern ecs_component_desc_t ecs_id(cname##_desc)
#define SIECS_COMPONENT_META_DEFINE(cname) SIJSON_DEFINE(cname)
#define SIECS_COMPONENT_META_INIT(cname) .struct_desc = &sireflect_desc(cname),
#define SIECS_TAG_META_DEFINE(cname)                                           \
  static const sireflect_struct_desc_t sireflect_desc(cname) = {               \
      .name = #cname, .fields = "{}", .size = 0, .align = 1};
/*
 * Define a component declared with ECS_COMPONENT_DECLARE.
 *
 * Use once in a C file:
 *   ECS_COMPONENT_DEFINE(Position);
 */
#define ECS_COMPONENT_DEFINE(cname, ...)                                       \
  SIECS_COMPONENT_META_DEFINE(cname)                                           \
  ecs_component_desc_t ecs_id(cname##_desc) = {                                \
      .name = #cname,                                                          \
      .size = sizeof(cname),                                                   \
      SIECS_COMPONENT_META_INIT(cname) __VA_ARGS__};                           \
  ecs_component_t ecs_id(cname) = 0

/*
 * Declare a tag component without declaring a zero-member struct.
 *
 * The opaque type keeps the component name visible to C/C++ tooling and
 * preserves typed helpers such as ecs_id(Tag), ecs_filter(Tag), and
 * ecs::entity::has<Tag>(). Tags intentionally have no reflected data or
 * storage column; their descriptor therefore has size zero.
 */
#define ECS_TAG_DECLARE(cname)                                                 \
  typedef struct cname##_tag_t cname;                                          \
  extern ecs_component_t ecs_id(cname);                                        \
  extern ecs_component_desc_t ecs_id(cname##_desc)

/* Define a tag component declared with ECS_TAG_DECLARE. */
#define ECS_TAG_DEFINE(cname)                                                  \
  SIECS_TAG_META_DEFINE(cname)                                                 \
  ecs_component_desc_t ecs_id(cname##_desc) = {                                \
      .name = #cname, .size = 0, SIECS_COMPONENT_META_INIT(cname)};            \
  ecs_component_t ecs_id(cname) = 0

/* Declare and define a tag component in one translation unit. */
#define ECS_TAG(cname)                                                         \
  ECS_TAG_DECLARE(cname);                                                      \
  ECS_TAG_DEFINE(cname)

/*
 * Register a component type in a world.
 *
 * Must be called before using the typed helpers for that component with this
 * world. Stores the generated component id in ecs_id(cname).
 */
#define ECS_COMPONENT_REGISTER(cname)                                          \
  ecs_component_register(&ecs_id(cname), &ecs_id(cname##_desc))

/*
 * Declare and define a component type in one translation unit.
 *
 * Example:
 *   ECS_COMPONENT(Position, { float x; float y; });
 */
#define ECS_COMPONENT(cname, ...)                                              \
  ECS_COMPONENT_DECLARE(cname, __VA_ARGS__);                                   \
  ECS_COMPONENT_DEFINE(cname);

#ifdef __cplusplus
#define ECS_MODULE_CPP_DECLARE(module_name)                                    \
  struct module_name {                                                         \
    using props_t = module_name##_props_t;                                     \
    static ecs_module_id_t *id_storage() noexcept {                            \
      return &ecs_id(module_name);                                             \
    }                                                                          \
    static ecs_module_import_t import_callback() noexcept {                    \
      return ecs_id(module_name##_import_wrapper);                             \
    }                                                                          \
    static constexpr const char *name() noexcept { return #module_name; }       \
  };
#else
#define ECS_MODULE_CPP_DECLARE(...)
#endif
/*
 * Declare a typed module.
 *
 * Use in a header:
 *   ECS_MODULE_DECLARE(physics, { float gravity; });
 *
 * This declares physics_props_t, the public module id symbol ecs_id(physics),
 * an import wrapper, and the user-defined import function:
 *   void physics_import(const physics_props_t *props);
 *
 * In C++, this also declares a module adapter type named physics. It can be
 * imported with ecs::import<physics>() or
 * ecs::import<physics>(physics::props_t{ ... });. The adapter uses the same
 * C module id and import wrapper, so C and C++ imports share one module.
 */
#ifdef __cplusplus
#define ECS_MODULE_DECLARE(module_name, ...)                                   \
  typedef struct module_name##_props_t __VA_ARGS__ module_name##_props_t;      \
  extern "C" {                                                                \
    extern ecs_module_id_t ecs_id(module_name);                                \
    void ecs_id(module_name##_import_wrapper)(const void *desc);               \
    void module_name##_import(const module_name##_props_t *props);             \
  }                                                                            \
  ECS_MODULE_CPP_DECLARE(module_name)
#else
#define ECS_MODULE_DECLARE(module_name, ...)                                   \
  typedef struct module_name##_props_t __VA_ARGS__ module_name##_props_t;      \
  extern ecs_module_id_t ecs_id(module_name);                                  \
  void ecs_id(module_name##_import_wrapper)(const void *desc);                 \
  void module_name##_import(const module_name##_props_t *props);
#endif

/*
 * Define a typed module declared with ECS_MODULE_DECLARE.
 *
 * Use once in a C file before implementing module_name_import.
 */
#define ECS_MODULE_DEFINE(module_name)                                         \
  ecs_module_id_t ecs_id(module_name) = 0;                                     \
  void ecs_id(module_name##_import_wrapper)(const void *desc) {                \
    module_name##_import((const module_name##_props_t *)desc);                 \
  }

/*
 * Import a typed module into a world.
 *
 * The first import calls module_name_import and stores the returned module id
 * in ecs_id(module_name). Later imports of the same module in the same world
 * return the existing id without calling module_name_import again; the first
 * props value wins.
 */
#define ECS_MODULE_IMPORT(module_name, ...)                                    \
  (ecs_id(module_name) = ecs_module_init(&(ecs_module_desc_t){                 \
       .name = #module_name,                                                   \
       .id = &ecs_id(module_name),                                             \
       .import = ecs_id(module_name##_import_wrapper),                         \
       .desc = &(module_name##_props_t)__VA_ARGS__,                            \
       .desc_size = sizeof(module_name##_props_t),                             \
   }))

/*
 * Register/import a module with a raw descriptor.
 *
 * Prefer ECS_MODULE_IMPORT for typed modules. Use this when module properties
 * or ids are built dynamically.
 */
#define ecs_module(...) ecs_module_init(&(ecs_module_desc_t)__VA_ARGS__)

/* Register/import a module descriptor and return its module id. */
SIECS_API ecs_module_id_t ecs_module_init(const ecs_module_desc_t *desc);

/*
 * Find an already imported module by its id storage.
 *
 * For typed modules, pass &ecs_id(module_name). Returns 0 if the
 * module has not been imported into this world.
 */

/* Enable systems and observers recorded during module import. */
SIECS_API void ecs_module_enable(ecs_module_id_t module);

/* Resolve a module id from stable id storage; returns 0 when not imported. */
SIECS_API ecs_module_id_t ecs_module_find(const ecs_module_id_t *id);

/* Return the registered module name. */
SIECS_API const char *ecs_module_name(ecs_module_id_t module);

/* Disable systems and observers recorded during module import. */
SIECS_API void ecs_module_disable(ecs_module_id_t module);

/* Return whether a module is currently enabled in this world. */
SIECS_API bool ecs_module_is_enabled(ecs_module_id_t module);

typedef enum {
  EcsRelationDense,
  EcsRelationByDepth,
  EcsRelationByTarget
} ecs_relation_storage_t;

/* Per-source relation data stored by Dense and ByDepth relations. */
typedef struct {
  ecs_entity_t entity;
  uint32_t source_index;
} ecs_relation_target_t;

typedef enum { EcsRemoveRelation, EcsDeleteSources } ecs_delete_target_t;

typedef struct {
  ecs_relation_storage_t storage;
  ecs_delete_target_t on_delete_target;
  bool acyclic;
} ecs_relation_desc_t;

typedef struct {
  const char *name;
  ecs_relation_desc_t desc;
} ecs_relation_info_t;

#define ECS_RELATION_DECLARE(name)                                             \
  extern ecs_relation_id_t ecs_rid(name);                                      \
  extern ecs_relation_desc_t ecs_rid(name##_desc)

#define ECS_RELATION_DEFINE(name, ...)                                         \
  ecs_relation_desc_t ecs_rid(name##_desc) = __VA_ARGS__;                      \
  ecs_relation_id_t ecs_rid(name) = 0

#define ECS_RELATION(name, ...)                                                \
  ECS_RELATION_DECLARE(name);                                                  \
  ECS_RELATION_DEFINE(name, __VA_ARGS__)

#define ECS_RELATION_REGISTER(name)                                            \
  ecs_relation_register(&ecs_rid(name), #name, &ecs_rid(name##_desc))

ECS_RELATION_DECLARE(ChildOf);

/* C++ declarations made after this header use the C relation id/descriptor. */
#ifdef __cplusplus
#undef ECS_RELATION_DECLARE
#define ECS_RELATION_DECLARE(name)                                             \
  extern "C++" {                                                              \
    struct name {};                                                            \
  }                                                                            \
  extern "C" {                                                                \
    extern ecs_relation_id_t ecs_rid(name);                                    \
    extern ecs_relation_desc_t ecs_rid(name##_desc);                           \
  }                                                                            \
  extern "C++" {                                                              \
    namespace ecs {                                                           \
    namespace detail {                                                        \
    template <> struct c_relation_traits<name> {                              \
      static constexpr bool value = true;                                      \
      static constexpr const char *relation_name() noexcept { return #name; }   \
      static auto id_storage() noexcept { return &ecs_rid(name); }             \
      static auto desc_storage() noexcept { return &ecs_rid(name##_desc); }    \
    };                                                                        \
    }                                                                         \
    }                                                                         \
  }
#endif

/* Register a runtime relation and return its world-local relation id. */
SIECS_API ecs_relation_id_t ecs_relation_init(const char *name,
                                              const ecs_relation_desc_t *desc);
/* Register a declared relation once and update its stable id storage. */
SIECS_API ecs_relation_id_t ecs_relation_register(
    ecs_relation_id_t *id, const char *name, const ecs_relation_desc_t *desc);

/* Builtin component for entity names; the world owns a copied value. */
ECS_COMPONENT_DECLARE(Name, { char *value; });

/* Builtin tag excluded from queries by default. */
ECS_TAG_DECLARE(Disabled);

/* Builtin tag for abstract entities. */
ECS_TAG_DECLARE(Abstract);

/* C++ declarations made after this header use the C id/descriptor directly. */
#ifdef __cplusplus
#undef ECS_COMPONENT_DECLARE
#define ECS_COMPONENT_DECLARE(cname, ...)                                      \
  SIJSON_DECLARE(cname, __VA_ARGS__)                                           \
  extern "C" {                                                                \
    extern ecs_component_t ecs_id(cname);                                      \
    extern ecs_component_desc_t ecs_id(cname##_desc);                          \
  }                                                                            \
  extern "C++" {                                                             \
  namespace ecs {                                                             \
  namespace detail {                                                          \
  template <> struct c_component_traits<cname> {                              \
    static constexpr bool value = true;                                        \
    static auto id_storage() noexcept { return &ecs_id(cname); }               \
    static auto desc_storage() noexcept { return &ecs_id(cname##_desc); }      \
  };                                                                          \
  }                                                                           \
  }                                                                           \
  }

#undef ECS_TAG_DECLARE
#define ECS_TAG_DECLARE(cname)                                                  \
  typedef struct cname##_tag_t cname;                                          \
  extern "C" {                                                                \
    extern ecs_component_t ecs_id(cname);                                      \
    extern ecs_component_desc_t ecs_id(cname##_desc);                          \
  }                                                                            \
  extern "C++" {                                                             \
  namespace ecs {                                                             \
  namespace detail {                                                          \
  template <> struct c_component_traits<cname> {                              \
    static constexpr bool value = true;                                        \
    static auto id_storage() noexcept { return &ecs_id(cname); }               \
    static auto desc_storage() noexcept { return &ecs_id(cname##_desc); }      \
  };                                                                          \
  }                                                                           \
  }                                                                           \
  }
#endif

/*
 * Register a component from an inline descriptor.
 *
 * Example:
 *   ecs_component_t Position = ecs_component({
 *       .name = "Position",
 *       .size = sizeof(Position)
 *   });
 */
#define ecs_component(...)                                                     \
  ecs_component_init(&(ecs_component_desc_t)__VA_ARGS__)

/* Register a component descriptor and return its component id. */
SIECS_API ecs_component_t ecs_component_init(const ecs_component_desc_t *desc);

/* Register a typed component descriptor using stable process-wide id storage.
 */
SIECS_API ecs_component_t
ecs_component_register(ecs_component_t *id, const ecs_component_desc_t *desc);

/* Return immutable component metadata stable until ecs_fini(), or NULL for an
 * invalid id. */
SIECS_API const ecs_component_info_t *
ecs_component_info(ecs_component_t component);

/* Return the number of component slots, including reserved id 0. */
SIECS_API uint32_t ecs_component_count(void);

/* Register a reflected component whose C layout is derived by Sireflect.
 * Returns 0 on error. */
SIECS_API ecs_component_t
ecs_component_dynamic_init(const ecs_dynamic_component_desc_t *desc);

/* Register a zero-sized reflected tag. Returns 0 on error. */
SIECS_API ecs_component_t ecs_tag_init(const char *name);

/* Return the registered component name. */
SIECS_API const char *ecs_component_name(ecs_component_t component);

/* Return the number of relation slots, including reserved id 0. */
SIECS_API uint32_t ecs_relation_count(void);

/* Return immutable relation metadata borrowed until the next relation
 * registration or ecs_fini().
 */
SIECS_API const ecs_relation_info_t *
ecs_relation_info(ecs_relation_id_t relation);

/* Look up a live entity by its registered name; returns 0 when absent. */
ecs_entity_t ecs_lookup(const char *key);

/* Return the current live handle for an entity index, or 0 when unavailable. */
SIECS_API ecs_entity_t ecs_entity_from_index(uint32_t index);

/* Create a new alive entity in world. world must not be NULL. */
SIECS_API ecs_entity_t ecs_new(void);

/* Create a new alive entity without reusing a previously freed entity index.
 * The returned index is greater than every index allocated so far. */
SIECS_API ecs_entity_t ecs_new_no_reuse(void);

/* Get the explicit entity name or a generated "(index, generation)" name. */
SIECS_API const char *ecs_entity_name(ecs_entity_t entity);

/* Begin deferring ECS mutations into the world's command buffer. */
SIECS_API void ecs_defer_begin(void);

/* End a defer scope. The outermost end flushes the command buffer. */
SIECS_API void ecs_defer_end(void);

/*
 * Return whether entity is alive in world.
 *
 * entity must be a handle created by this world. Passing arbitrary ids is not a
 * supported validity check.
 */
SIECS_API bool ecs_is_alive(const ecs_entity_t entity);

/* Return whether entity is target or inherits from target. Both handles must
 * belong to the active world; zero or stale handles return false. */
SIECS_API bool ecs_is(ecs_entity_t entity, ecs_entity_t target);

/* Return the direct IsA base of an alive entity, or 0 when it has none. */
SIECS_API ecs_entity_t ecs_entity_base(ecs_entity_t entity);

/*
 * Add an inheritance link from entity to target; both handles must be live.
 * The target is made Abstract automatically.
 */
SIECS_API void ecs_is_a(ecs_entity_t entity, ecs_entity_t target);

#define ecs_relate(entity, relation, target)                                   \
  ecs_relate_id(entity, ecs_rid(relation), target)
#define ecs_unrelate(entity, relation)                                         \
  ecs_unrelate_id(entity, ecs_rid(relation))
#define ecs_has_relation(entity, relation)                                     \
  ecs_has_relation_id(entity, ecs_rid(relation))
#define ecs_has_relation_to(entity, relation, target)                          \
  ecs_has_relation_to_id(entity, ecs_rid(relation), target)
#define ecs_target(entity, relation) ecs_target_id(entity, ecs_rid(relation))

/* Add or retarget one relation edge. In Debug, both entities must be alive. */
SIECS_API void ecs_relate_id(ecs_entity_t entity, ecs_relation_id_t relation,
                             ecs_entity_t target);
/* Remove one relation edge; this is a no-op when the source has no such edge.
 */
SIECS_API void ecs_unrelate_id(ecs_entity_t entity, ecs_relation_id_t relation);
/* Return whether the source has an edge for relation. */
SIECS_API bool ecs_has_relation_id(ecs_entity_t entity,
                                   ecs_relation_id_t relation);
/* Return whether the source edge has exactly target, including its generation.
 */
SIECS_API bool ecs_has_relation_to_id(ecs_entity_t entity,
                                      ecs_relation_id_t relation,
                                      ecs_entity_t target);
/* Return the source edge target, or zero when absent. */
SIECS_API ecs_entity_t ecs_target_id(ecs_entity_t entity,
                                     ecs_relation_id_t relation);

/* Return a relation target stored in a ByTarget table. */
SIECS_API ecs_entity_t ecs_table_target_id(const ecs_table_t *table,
                                           ecs_relation_id_t relation);
#define ecs_table_target(table, relation) \
  ecs_table_target_id(table, ecs_rid(relation))

/* Return whether a component is available on a table, including its base. */
SIECS_API bool ecs_table_has_id(const ecs_table_t *table, ecs_component_t component);

/* Build standard table order descriptors. */
SIECS_API ecs_query_order_t ecs_order_by_target_id(ecs_relation_id_t relation);
SIECS_API ecs_query_order_t ecs_order_by_depth_id(ecs_relation_id_t relation);

/* Destroy an alive entity and remove all of its components. */
SIECS_API void ecs_kill(ecs_entity_t entity);

/*
 * Create a query from an inline descriptor.
 *
 * Example:
 *   ecs_query_id_t q = ecs_query({
 *       .terms = { ecs_in(Position), ecs_in(Velocity) }
 *   });
 */
#define ecs_query(...) ecs_query_init(&(ecs_query_desc_t)__VA_ARGS__)

/*
 * Convenience query loop for short-lived queries.
 *
 * This macro creates a temporary query, iterates it, then destroys it. Prefer a
 * persistent ecs_query_id_t for systems, hot paths, or repeated frame work.
 *
 * Example:
 *   ecs_query_each(it, i, ecs_in(Position), ecs_in(Velocity)) {
 *       Position *p = ecs_field(&it, 0);
 *       Velocity *v = ecs_field(&it, 1);
 *       p[i].x += v[i].x;
 *   }
 */
#define ecs_query_each(it, i, ...)                                             \
  for (ecs_query_id_t _q = ecs_query({{__VA_ARGS__}}); _q;                     \
       ecs_query_fini(_q), _q = 0)                                             \
    for (ecs_iter_t it = ecs_query_iter(_q); ecs_iter_next(&it);)              \
      for (uint32_t i = 0; i < it.count; i++)

/* Iterate entity ids instead of exposing component fields. */
#define ecs_query_entities(entity, ...)                                        \
  for (ecs_query_id_t _q = ecs_query({{__VA_ARGS__}}); _q;                     \
       ecs_query_fini(_q), _q = 0)                                             \
    for (ecs_iter_t it = ecs_query_iter(_q); ecs_iter_next(&it);)              \
      for (uint64_t i = 0, entity = *it.entities; i < it.count;                \
           i++, entity = it.entities[i])
/* Create a query. The query descriptor must read at least one component. */
SIECS_API ecs_query_id_t ecs_query_init(const ecs_query_desc_t *query);

/* Destroy a query id created by ecs_query/ecs_query_init. */
SIECS_API void ecs_query_fini(ecs_query_id_t qid);

/* Add a typed component tag/storage to an alive entity. */
#define ecs_add(entity, cname) ecs_add_cid(entity, ecs_id(cname))

/*
 * Add a component id to an alive entity.
 *
 * If the component is already present, this is currently treated as a no-op by
 * table migration. The component id must be registered in the same world.
 */
SIECS_API void ecs_add_cid(ecs_entity_t entity, ecs_component_t id);

/* Remove a typed component from an alive entity. */
#define ecs_remove(entity, cname) ecs_remove_cid(entity, ecs_id(cname))

/*
 * Remove a component id from an alive entity.
 *
 * Removing a component that is not present is a no-op.
 */
SIECS_API void ecs_remove_cid(ecs_entity_t entity, ecs_component_t id);

/* Return whether an alive entity has a typed component. */
#define ecs_has(entity, cname) ecs_has_cid(entity, ecs_id(cname))

/* Return whether an alive entity has a component id. */
SIECS_API bool ecs_has_cid(const ecs_entity_t entity, ecs_component_t id);
/* Return whether the entity owns the component rather than inheriting it. */
bool ecs_has_cid_owned(const ecs_entity_t entity, ecs_component_t id);

/*
 * Get a typed component pointer from an alive entity.
 *
 * The component is assumed to exist. Use ecs_try_get when the component may be
 * absent.
 */
#define ecs_get(entity, cname) ((cname *)ecs_get_cid(entity, ecs_id(cname)))

/*
 * Get a component pointer by id.
 *
 * The component is assumed to exist on the entity. Use ecs_try_get_cid when the
 * component may be absent.
 */
SIECS_API void *ecs_get_cid(ecs_entity_t entity, ecs_component_t id);

/* Get a typed component pointer, or NULL if the entity does not have it. */
#define ecs_try_get(entity, cname)                                             \
  ((cname *)ecs_try_get_cid(entity, ecs_id(cname)))

/*
 * Get an owned or inherited component pointer by id.
 * The entity must be valid and alive; returns NULL only when the component is
 * absent.
 */
SIECS_API void *ecs_try_get_cid(ecs_entity_t entity, ecs_component_t cid);

/*
 * Set a typed component value on an alive entity.
 *
 * Adds the component if needed, then emits EcsOnSet. Component on_set hooks
 * receive the new value and current storage before the copy. EcsOnSet observers
 * receive the new value before it is copied into storage.
 */
#define ecs_set(entity, cname, ...)                                            \
  ecs_set_cid(entity, ecs_id(cname), &(cname)__VA_ARGS__)

/*
 * Set a component value by id.
 *
 * data must point to at least the registered component size. Adds the component
 * if needed.
 */
SIECS_API void ecs_set_cid(ecs_entity_t entity, ecs_component_t id,
                           const void *data);
/* Move a component value into an entity, consuming data with the registered
 * move operation. data must point to an initialized value of the component. */
SIECS_API void ecs_move_cid(ecs_entity_t entity, ecs_component_t id,
                            void *data);

/*
 * Declare and define a resource type.
 *
 * Resources use their own id space and are stored once per world. They are not
 * components, do not appear in queries, and do not consume component ids.
 *
 * Use ECS_RESOURCE_DECLARE in headers and ECS_RESOURCE_DEFINE once in a C file,
 * or ECS_RESOURCE for local examples/tests.
 *
 * A resource declared this way can also be passed directly to the C++ resource
 * helpers. C ids, descriptors, lifecycle operations, and hooks remain the
 * source of truth.
 *
 * Example:
 *   ECS_RESOURCE(Time, { float dt; float elapsed; });
 */
#ifdef __cplusplus
#define ECS_RESOURCE_DECLARE(rname, ...)                                       \
  typedef struct rname rname;                                                  \
  struct rname __VA_ARGS__;                                                    \
  extern "C" {                                                                \
    extern ecs_resource_t ecs_id(rname);                                       \
    extern ecs_resource_desc_t ecs_id(rname##_desc);                           \
  }                                                                            \
  extern "C++" {                                                             \
  namespace ecs {                                                             \
  namespace detail {                                                          \
  template <> struct c_resource_traits<rname> {                               \
    static constexpr bool value = true;                                        \
    static auto id_storage() noexcept { return &ecs_id(rname); }               \
    static auto desc_storage() noexcept { return &ecs_id(rname##_desc); }      \
  };                                                                          \
  }                                                                           \
  }                                                                           \
  }
#else
#define ECS_RESOURCE_DECLARE(rname, ...)                                       \
  typedef struct rname rname;                                                  \
  struct rname __VA_ARGS__;                                                    \
  extern ecs_resource_t ecs_id(rname);                                         \
  extern ecs_resource_desc_t ecs_id(rname##_desc)
#endif

/* Define a resource descriptor and its stable id storage. */
#define ECS_RESOURCE_DEFINE(rname, ...)                                        \
  ecs_resource_desc_t ecs_id(rname##_desc) = {                                 \
      .name = #rname, .size = sizeof(rname), __VA_ARGS__};                     \
  ecs_resource_t ecs_id(rname) = 0

/* Register a declared resource in the active world. */
#define ECS_RESOURCE_REGISTER(rname)                                           \
  ecs_resource_register(&ecs_id(rname), &ecs_id(rname##_desc))

/* Declare and define a resource in one translation unit. */
#define ECS_RESOURCE(rname, ...)                                               \
  ECS_RESOURCE_DECLARE(rname, __VA_ARGS__);                                    \
  ECS_RESOURCE_DEFINE(rname)

/*
 * Set or replace a world resource.
 *
 * Example:
 *   ecs_set_resource(Time, { .dt = 0.016f, .elapsed = 0.0f });
 */
#define ecs_set_resource(rname, ...)                                           \
  ecs_set_resource_rid(ecs_id(rname), &(rname)__VA_ARGS__)

/* Get a world resource. The resource must exist. */
#define ecs_get_resource(rname) ((rname *)ecs_resource_rid(ecs_id(rname)))
/* Get a const world resource. The resource must exist. */
#define ecs_get_resource_read(rname)                                           \
  ((const rname *)ecs_resource_rid(ecs_id(rname)))

/* Get a world resource, or NULL if it does not exist. */
#define ecs_try_get_resource(rname)                                            \
  ((rname *)ecs_try_resource_rid(ecs_id(rname)))
/* Get a const world resource, or NULL if it does not exist. */
#define ecs_try_get_resource_read(rname)                                       \
  ((const rname *)ecs_try_resource_rid(ecs_id(rname)))

/* Return whether a world resource exists. */
#define ecs_has_resource(rname) ecs_has_resource_rid(ecs_id(rname))

/* Remove a world resource if it exists. */
#define ecs_remove_resource(rname) ecs_remove_resource_rid(ecs_id(rname))

/* Deprecated compatibility aliases. Prefer ecs_get_resource* in new code;
 * removal requires a major release. */
#define ecs_resource(rname) ecs_get_resource(rname)
/* Deprecated read-only alias for ecs_get_resource_read. */
#define ecs_resource_read(rname) ecs_get_resource_read(rname)
/* Deprecated nullable alias for ecs_try_get_resource. */
#define ecs_try_resource(rname) ecs_try_get_resource(rname)
/* Deprecated nullable read-only alias for ecs_try_get_resource_read. */
#define ecs_try_resource_read(rname) ecs_try_get_resource_read(rname)

/* Register a resource descriptor in the active world and return its id. */
SIECS_API ecs_resource_t ecs_resource_init(const ecs_resource_desc_t *desc);
/* Find a registered resource by name; returns 0 when absent. */
SIECS_API ecs_resource_t ecs_resource_find(const char *name);
/* Return the registered resource name; pointer remains owned by the world. */
SIECS_API const char *ecs_resource_name(ecs_resource_t resource);
/* Return whether a resource id is registered in the active world. */
SIECS_API bool ecs_resource_is_registered_rid(ecs_resource_t id);
/* Register a resource using stable id storage; returns the resulting id. */
SIECS_API ecs_resource_t ecs_resource_register(ecs_resource_t *id,
                                               const ecs_resource_desc_t *desc);
/* Copy or move data into a registered resource. */
SIECS_API void ecs_set_resource_rid(ecs_resource_t id, const void *data);
/* Move an initialized resource value into storage and consume the source. */
SIECS_API void ecs_move_resource_rid(ecs_resource_t id, void *data);
/* Return a resource pointer (resource must exist), or NULL for try access. */
SIECS_API void *ecs_resource_rid(ecs_resource_t id);
/* Return resource storage or NULL when the resource is absent. */
SIECS_API void *ecs_try_resource_rid(ecs_resource_t id);
/* Test/remove a resource in the active world. */
SIECS_API bool ecs_has_resource_rid(const ecs_resource_t id);
/* Remove a resource and run its destructor/hook; no-op when absent. */
SIECS_API void ecs_remove_resource_rid(ecs_resource_t id);

/*
 * Declare that adding component also adds require first.
 *
 * Requirement cycles are debug assertion failures when declared.
 *
 * Example:
 *   ecs_with(ecs_id(Renderable), ecs_id(Transform));
 *   ecs_add(entity, Renderable); // also adds Transform
 */
SIECS_API void ecs_with(ecs_component_t component, ecs_component_t require);

/* Builtin observer events. */
#define EcsOnAdd 0
/* Fired after a component is removed. */
#define EcsOnRemove 1
/* Fired when a component is set or replaced. */
#define EcsOnSet 2
/* Fired after a relation is added or retargeted. */
#define EcsOnRelationSet 3
/* Fired before a relation is removed. */
#define EcsOnRelationRemove 4

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
 *   ecs_observer({
 *       .on = EcsOnSet,
 *       .query.terms = { ecs_in(Position) },
 *       .callback = on_position_set,
 *   });
 */
/* Create an observer from a compound-literal descriptor. */
#define ecs_observer(...) ecs_observer_init(&(ecs_observer_desc_t)__VA_ARGS__)

/* Allocate and return a custom event id for ecs_observer_trigger. */
SIECS_API ecs_event_t ecs_event(void);

/*
 * Register a process-wide typed event id in this world.
 *
 * If *id is UINT16_MAX, allocates a new custom event id and stores it in *id.
 * Otherwise, reserves the existing id in this world so future ecs_event calls
 * cannot collide with the typed event.
 */
SIECS_API ecs_event_t ecs_event_register(ecs_event_t *id);

/* Create an observer. desc->callback must not be NULL. */
SIECS_API ecs_observer_id_t ecs_observer_init(const ecs_observer_desc_t *desc);

/* Enable or disable an observer; disabled observers remain registered. */
SIECS_API void ecs_observer_enable(ecs_observer_id_t id);
/* Disable an observer without destroying its registration. */
SIECS_API void ecs_observer_disable(ecs_observer_id_t id);

/*
 * Trigger an event for an alive entity.
 *
 * Observers matching the entity's current table and event id will be called.
 */
SIECS_API void ecs_observer_trigger(ecs_entity_t entity, ecs_event_t event,
                                    const void *trigger_data);

/*
 * Query iterator.
 *
 * Users may read count. The other fields are implementation details
 * and should not be accessed directly.
 *
 * entities points to the current batch after ecs_iter_next returns true.
 */
typedef enum {
  EcsFieldNone,
  EcsFieldOwned,
  EcsFieldShared,
} ecs_field_kind_t;

/* Iterator storage returned by ecs_query_iter; ptrs/entities are batch views.
 */
typedef struct {
  uint32_t count;
  ecs_entity_t *entities;
  void **ptrs;
  float delta_time;
  struct ecs_query_cache_s *cache;
  uint32_t field_kind_bits;
  uintptr_t user_data;
  uint16_t table_idx;
  uint16_t table_count;
} ecs_iter_t;

/*
 * Create a stack iterator for a query id.
 *
 * Example:
 *   ecs_iter_t it = ecs_query_iter(query);
 *   while (ecs_iter_next(&it)) {
 *       Position *p = ecs_field(&it, 0);
 *   }
 */
SIECS_API ecs_iter_t ecs_query_iter(ecs_query_id_t query_id);

/* Return the number of entities currently matching a query. */
SIECS_API uint32_t ecs_query_count(ecs_query_id_t query_id);

/*
 * Advance an iterator to the next non-empty batch.
 *
 * Returns false when iteration is finished. it->count is the number of entities
 * in the current batch.
 */
SIECS_API bool ecs_iter_next(ecs_iter_t *it);
/* Return a relation target for one row of the current iterator batch. */
SIECS_API ecs_entity_t ecs_target_at_id(const ecs_iter_t *it,
                                        ecs_relation_id_t relation,
                                        uint32_t row);
#define ecs_target_at(it, relation, row)                                       \
  ecs_target_at_id(it, ecs_rid(relation), row)
/* Return contiguous relation target records for a Dense or ByDepth batch. */
SIECS_API const ecs_relation_target_t *ecs_targets_id(const ecs_iter_t *it,
                                                      ecs_relation_id_t relation);
#define ecs_targets(it, relation) ecs_targets_id(it, ecs_rid(relation))

/*
 * Return the component array for a read term in the current iterator batch.
 *
 * field_index is zero-based among EcsIn, EcsOut, EcsInOut, EcsInOptional and
 * EcsInOutOptional terms only. Optional fields return NULL when the current
 * table does not have the component. EcsFilter and EcsNot terms affect matching
 * but are not returned as fields.
 */
static inline ecs_field_kind_t ecs_field_kind(const ecs_iter_t *it,
                                              uint16_t field_index) {
  ecs_field_kind_t kind =
      (ecs_field_kind_t)((it->field_kind_bits >> (field_index * 2)) & 0x3u);
  return kind;
}

/* Return the current field pointer; call only after ecs_iter_next() succeeds.
 */
static inline void *ecs_field(ecs_iter_t *it, uint16_t field_index) {
  return it->ptrs[field_index];
}

/* Return whether a current field is inherited/shared rather than owned. */
static inline bool ecs_field_is_shared(ecs_iter_t *it, uint16_t field_index) {
  return ecs_field_kind(it, field_index) == EcsFieldShared;
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

/* Deprecated compatibility aliases. Prefer Ecs* names in new code; removal
 * requires a major release. */
#define OnPreUpdate EcsPreUpdate
/* Deprecated alias for EcsOnUpdate. */
#define OnUpdate EcsOnUpdate
/* Deprecated alias for EcsPostUpdate. */
#define OnPostUpdate EcsPostUpdate
/* Deprecated alias for EcsOnRender. */
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
  uintptr_t user_data;
  void (*user_data_dtor)(uintptr_t user_data);
  ecs_phase_t phase;
  ecs_system_id_t after[ECS_SYSTEM_AFTER_CAPACITY];
  bool disabled;
} ecs_system_desc_t;

/*
 * Create a system from an inline descriptor.
 *
 * Example:
 *   ecs_system({
 *       .name = "Move",
 *       .phase = EcsOnUpdate,
 *       .query.terms = { ecs_inout(Position), ecs_in(Velocity) },
 *       .callback = Move,
 *   });
 */
/* Create a system from a compound-literal descriptor. */
#define ecs_system(...) ecs_system_init(&(ecs_system_desc_t)__VA_ARGS__)

/* Register a system and return its id. System id 0 is reserved. */
SIECS_API ecs_system_id_t ecs_system_init(const ecs_system_desc_t *desc);

/* Return the registered system name. */
SIECS_API const char *ecs_system_name(ecs_system_id_t system);

/* Run all enabled systems in phase order. */
SIECS_API bool ecs_progress(void);

/* Run all enabled systems in phase order. */
SIECS_API void ecs_run(void);

/* Run all enabled systems from one phase. */
SIECS_API void ecs_run_phase(ecs_phase_t phase);

/* Run one enabled system immediately. */
SIECS_API void ecs_run_system(ecs_system_id_t system);

/* Enable or disable a system. Disabled systems stay registered but do not run.
 */
SIECS_API void ecs_system_enable(ecs_system_id_t system);
/* Disable a system without unregistering it. */
SIECS_API void ecs_system_disable(ecs_system_id_t system);

#ifdef __cplusplus
}
#endif

#if defined(__cplusplus) && !defined(SIECS_NO_CPP)
#include "siecs/cpp.hpp"
#endif

#endif
