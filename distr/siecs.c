#include "siecs.h"
#ifndef SIECS_STORAGE_TABLE_INDEX_H
#define SIECS_STORAGE_TABLE_INDEX_H
#ifndef SIECS_TABLE_H
#define SIECS_TABLE_H
#ifndef SIECS_DATASTRUCTURE_IDMAP_H
#define SIECS_DATASTRUCTURE_IDMAP_H
#include <stdint.h>

typedef struct {
    uint16_t *ids;
    uint16_t capacity;
} ecs_id_map_t;

void ecs_id_map_init(ecs_id_map_t *map);
void ecs_id_map_fini(ecs_id_map_t *map);

void ecs_id_map_ensure(ecs_id_map_t *map, uint16_t id);

static inline void ecs_id_map_set(ecs_id_map_t *map, uint16_t id, uint16_t value) {
    ecs_id_map_ensure(map, id);
    map->ids[id] = value;
}

static inline uint16_t ecs_id_map_at(const ecs_id_map_t *map, uint16_t id) { return map->ids[id]; }

static inline uint16_t ecs_id_map_at_or_invalid(const ecs_id_map_t *map, uint16_t id) {
    return map->capacity > id ? map->ids[id] : UINT16_MAX;
}

static inline uint16_t ecs_id_map_has(const ecs_id_map_t *map, uint16_t id) {
    return map->capacity > id && map->ids[id] != UINT16_MAX;
}

#endif

#ifndef SIECS_DATASTRUCTURE_VEC_H
#define SIECS_DATASTRUCTURE_VEC_H
#include <stdbool.h>
#include <stdint.h>
#ifndef SIECS_COMPILER_H
#define SIECS_COMPILER_H
#define ECS_LIKELY(x) __builtin_expect(!!(x), 1)
#define ECS_UNLIKELY(x) __builtin_expect(!!(x), 0)

#endif

typedef struct {
    void *data;
    uint32_t size;
    uint32_t capacity;
} ecs_vec_t;

void ecs_vec_init(ecs_vec_t *vec, const uint32_t element_size);
void ecs_vec_fini(ecs_vec_t *vec);
void ecs_vec_grow(ecs_vec_t *vec, const uint32_t element_size);
void ecs_vec_resize_max(ecs_vec_t *vec, uint32_t new_capacity, const uint32_t element_size);

// Ensure vec has at least `count` elements. New slots are zero-initialized.
void ecs_vec_ensure(ecs_vec_t *vec, uint32_t count, const uint32_t element_size);

// Copy element into the vec (memcpy). The pointer is not retained.
// Safe to call repeatedly — any grow only invalidates the internal buffer, not
// the source pointer.
void ecs_vec_push(ecs_vec_t *vec, const void *element, const uint32_t element_size);

// Reserve one slot and return a pointer to it (uninitialized).
// WARNING: the returned pointer is invalidated by any subsequent push or grow
// on the same vec. Finish all writes through this pointer before pushing again.
static inline void *ecs_vec_push_empty(ecs_vec_t *vec, const uint32_t element_size) {
    if (ECS_UNLIKELY(vec->size >= vec->capacity)) {
        ecs_vec_grow(vec, element_size);
    }
    void *ptr = (uint8_t *)vec->data + (vec->size * element_size);
    vec->size++;
    return ptr;
}

bool ecs_vec_contains_u64(const ecs_vec_t *vec, uint64_t value);
void ecs_vec_remove_u64(ecs_vec_t *vec, uint64_t value);

// Specialized push for 2-byte types
static inline void ecs_vec_push_u16(ecs_vec_t *vec, const uint16_t value) {
    if (ECS_UNLIKELY(vec->size >= vec->capacity)) {
        ecs_vec_grow(vec, sizeof(uint16_t));
    }
    ((uint16_t *)vec->data)[vec->size++] = value;
}

// Specialized push for 4-byte types
static inline void ecs_vec_push_u32(ecs_vec_t *vec, const uint32_t value) {
    if (ECS_UNLIKELY(vec->size >= vec->capacity)) {
        ecs_vec_grow(vec, sizeof(uint32_t));
    }
    ((uint32_t *)vec->data)[vec->size++] = value;
}

// Specialized push for 8-byte types
static inline void ecs_vec_push_u64(ecs_vec_t *vec, const uint64_t value) {
    if (ECS_UNLIKELY(vec->size >= vec->capacity)) {
        ecs_vec_grow(vec, sizeof(uint64_t));
    }
    ((uint64_t *)vec->data)[vec->size++] = value;
}

void ecs_vec_remove_fast(ecs_vec_t *vec, uint32_t index, const uint32_t element_size);

// Direct pointer access for fast iteration
#define ecs_vec_get(vec, index, type) (&((const type *)(vec)->data)[index])
#define ecs_vec_get_mut(vec, index, type) (&((type *)(vec)->data)[index])
#define ecs_vec_get_last(vec, type) (&((type *)(vec)->data)[(vec)->size - 1])
#define ecs_vec_remove_last(vec) ((vec)->size--)
#define ecs_vec_clear(vec) ((vec)->size = 0)
#define ecs_vec_data(vec, type) ((type *)(vec)->data)

// Direct indexed access for fast writes. Does not check bounds or grow the vec.
#define ecs_vec_set(vec, type, index, value) (((type *)vec->data)[index] = value)

#define ecs_vec_iter(vec, type, value, ...)                                                        \
    const type *__values = (vec)->data;                                                            \
    const uint32_t __count = (vec)->size;                                                          \
    for (uint32_t i = 0; i < __count; i++) {                                                       \
        const type *value = &__values[i];                                                          \
        __VA_ARGS__                                                                                \
    }

#define ecs_vec_iter_mut(vec, type, value, ...)                                                    \
    type *__values = (vec)->data;                                                                  \
    const uint32_t __count = (vec)->size;                                                          \
    for (uint32_t i = 0; i < __count; i++) {                                                       \
        type *value = &__values[i];                                                                \
        __VA_ARGS__                                                                                \
    }

#endif

#ifndef SIECS_ID_H
#define SIECS_ID_H
#include <stdint.h>

#define ecs_entity(index, generation) (((uint64_t)(index) << 32) | (generation & 0xffffffff))

#define ecs_first(id) ((uint32_t)((id) >> 32))
#define ecs_second(id) ((uint32_t)((id) & 0xffffffff))

typedef uint64_t ecs_entity_t;
typedef uint16_t ecs_component_t;

#endif

#ifndef SIECS_TYPE_H
#define SIECS_TYPE_H
#include <stdint.h>
#include <string.h>

typedef struct {
    uint16_t *ids;
    uint16_t count;
} ecs_type_t;

ecs_type_t ecs_type_with_add(const ecs_type_t *type, uint16_t id);
ecs_type_t ecs_type_with_remove(const ecs_type_t *type, uint16_t id);

uint64_t ecs_type_bloom(const ecs_type_t *type);

// returns the index of the id in the type, or -1 if not found
int ecs_type_find(const ecs_type_t *type, uint16_t id);

void ecs_type_fini(ecs_type_t *type);

static inline int
ecs_type_equals(const uint16_t *a_ids, uint16_t a_count, const uint16_t *b_ids, uint16_t b_count) {
    if (a_count != b_count)
        return 0;
    if (a_count == 0)
        return 1;
    return memcmp(a_ids, b_ids, (size_t)a_count * sizeof(uint16_t)) == 0;
}

#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    void *data;
    uint32_t size;
    uint16_t remove_edge; // the table that has the component removed or UINT16_MAX if the edge is not set
} ecs_column_t;

typedef struct ecs_table_s {
    ecs_id_map_t add_edge; // maps component id to the table that has the component added or column
                           // index if the component is in the table
    uint32_t entity_capacity;
    uint32_t entity_count;
    ecs_entity_t *entities;
    ecs_column_t *cls;
    ecs_type_t type;
    uint64_t bloom;
    ecs_vec_t observers_by_event; // ecs_vec_t per event id; each holds uint16_t observer ids.
} ecs_table_t;

struct ecs_component_index_s;

void ecs_table_init(
    ecs_table_t *table,
    ecs_type_t type,
    const struct ecs_component_index_s *component_index,
    uint16_t table_id
);
void ecs_table_fini(ecs_table_t *table);
uint32_t ecs_table_add_entity(ecs_table_t *table, ecs_entity_t entity);
// if the entity is not the last one, the last entity will be moved to the removed entity's
// position, and the moved entity will be returned
ecs_entity_t ecs_table_remove_entity(ecs_table_t *table, uint32_t row);

void *ecs_table_get_component(ecs_table_t *table, ecs_component_t component_id, uint32_t row);

// Append an observer id to this table's dense list for the given event,
// growing the per-event slot array on demand.
void ecs_table_add_observer(ecs_table_t *table, uint16_t event, uint16_t observer_id);

static inline uint16_t
ecs_table_get_add_edge(const ecs_table_t *table, ecs_component_t component_id) {
    return ecs_id_map_at_or_invalid(&table->add_edge, component_id);
}

static inline bool ecs_table_has(const ecs_table_t *table, ecs_component_t component_id) {
    uint16_t column_index = ecs_table_get_add_edge(table, component_id);
    return (bool)((column_index < table->type.count) &&
                  (table->type.ids[column_index] == component_id));
}

static inline uint16_t
ecs_table_get_column_index(const ecs_table_t *table, ecs_component_t component_id) {
    return ecs_id_map_at(&table->add_edge, component_id);
}

static inline uint16_t ecs_table_get_remove_edge(const ecs_table_t *table, ecs_component_t component_id) {
    return table->cls[ecs_table_get_column_index(table, component_id)].remove_edge;
}

#endif

#include <stdint.h>

typedef struct {
    uint16_t table_index; // UINT16_MAX for empty
    uint16_t hash;
} ecs_type_slot_t;

typedef struct {
    ecs_table_t *tables;
    ecs_type_slot_t *slots;
    uint16_t table_count;
    uint16_t table_capacity;
    uint8_t slot_shift; // slot_count = 1 << slot_shift
} ecs_table_index_t;

void ecs_table_index_init(ecs_table_index_t *map);
void ecs_table_index_fini(ecs_table_index_t *map);

#define ecs_table_index_at(map, index) (&(map)->tables[index])

struct ecs_world_s;
uint16_t ecs_table_index_get_or_create(
    struct ecs_world_s *world,
    ecs_type_t type
);

#endif

ECS_RELATION_DEFINE(ChildOf);

void ecs_bootstrap(ecs_world_t *world) {
    // Reserve identifiers used to represent false return values.
    ecs_table_index_get_or_create(world, (ecs_type_t){ 0 });
    ecs_new(world);
    ecs_component(world, {});

    ECS_COMPONENT_REGISTER(world, ChildOf);
}

#ifndef SIECS_STORAGE_COMPONENT_INDEX_H
#define SIECS_STORAGE_COMPONENT_INDEX_H
#ifndef SIECS_DATASTRUCTURE_MAP_H
#define SIECS_DATASTRUCTURE_MAP_H
#ifndef NDEBUG

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    const char *key;
    uint64_t hash;
    uint32_t value;
} ecs_map_slot_t;

typedef struct {
    ecs_map_slot_t *slots;
    size_t cap;
    size_t len;
} ecs_map_t;

void ecs_map_init(ecs_map_t *m, size_t initial_capacity);
void ecs_map_fini(ecs_map_t *m);

void ecs_map_set(ecs_map_t *m, const char *key, uint32_t value);
uint32_t ecs_map_get(const ecs_map_t *m, const char *key);
bool ecs_map_has(const ecs_map_t *m, const char *key);
#endif

#endif

#include <stdint.h>

typedef struct {
    const char *name;
    uint16_t *required;
    uint32_t required_count;
    uint32_t size;
    ecs_component_hook_t on_set;
    ecs_component_hook_t on_remove;
    ecs_vec_t tables; // uint16_t
} ecs_component_record_t;

typedef struct ecs_component_index_s {
    ecs_vec_t components; // ecs_component_record_t
    #ifndef NDEBUG
    ecs_map_t component_name_map;
    #endif
} ecs_component_index_t;

ecs_component_t ecs_component_index_create(
    ecs_component_index_t *index,
    const char *name,
    uint64_t size,
    ecs_component_hook_t on_set,
    ecs_component_hook_t on_remove
);

#define ecs_component_index_get(index, id)                                                         \
    ecs_vec_get(&(index)->components, id, ecs_component_record_t)
#define ecs_component_index_get_mut(index, id)                                                         \
    ecs_vec_get_mut(&(index)->components, id, ecs_component_record_t)

void ecs_component_index_init(ecs_component_index_t *index);
void ecs_component_index_fini(ecs_component_index_t *index);

#endif

#ifndef SIECS_UTILS_H
#define SIECS_UTILS_H
#ifndef NDEBUG
#include <stdio.h>
#include <stdlib.h>
#define ecs_cid_valid(id) ((id) != 0)
#define ecs_entity_valid(entity) (ecs_first(entity) != 0)

#define ecs_assert(condition, ...) \
    if (!(condition)) { \
        fprintf(stderr, __VA_ARGS__); \
        abort(); \
    }

#define ecs_assert_id_valid(id) ecs_assert(ecs_cid_valid(id), "invalid id: %d, id must be registered\n", id)
#define ecs_assert_not_null(ptr) ecs_assert((ptr) != NULL, "null pointer: %s\n", #ptr)
#define ecs_assert_entity_valid(entity) ecs_assert(ecs_entity_valid(entity), "invalid entity: %d, entity must be registered\n", ecs_first(entity))
#define ecs_assert_is_alive(world, entity) ecs_assert(ecs_is_alive(world, entity), "entity is dead: %d\n", ecs_first(entity))

#else
#define ecs_assert(condition, ...)
#define ecs_assert_id_valid(id)
#define ecs_assert_not_null(ptr)
#define ecs_assert_entity_valid(entity)
#define ecs_assert_is_alive(world, entity)
#endif

#endif

#ifndef SIECS_WORLD_INTERNAL_H
#define SIECS_WORLD_INTERNAL_H
#ifndef SIECS_STORAGE_ENTITY_INDEX_H
#define SIECS_STORAGE_ENTITY_INDEX_H
#include <stdint.h>

typedef struct {
    uint16_t generation;
    uint16_t table_id;
    // Alive records store the row in their table. Dead records reuse this field
    // as the next entity id in the free list headed by first_available.
    uint32_t table_row;
} ecs_entity_record_t;

typedef struct {
    ecs_vec_t entities;        // ecs_entity_record_t
    uint32_t first_available;  // UINT32_MAX when no dead entity can be reused
} ecs_entity_index_t;

#define ecs_entity_index_get_record(index, entity_id)                                              \
    ecs_vec_get_mut((&(index)->entities), entity_id, ecs_entity_record_t)

static inline ecs_entity_t ecs_entity_index_create(ecs_entity_index_t *index, uint32_t row) {
    uint32_t entity_id;
    uint32_t generation;
    if (index->first_available != UINT32_MAX) {
        entity_id = index->first_available;
        ecs_entity_record_t *record = ecs_entity_index_get_record(index, entity_id);
        index->first_available = record->table_row;
        generation = record->generation;
        record->table_id = 0;
        record->table_row = row;
    } else {
        entity_id = index->entities.size;
        generation = 0;
        ecs_entity_record_t *record = (ecs_entity_record_t *)
            ecs_vec_push_empty(&index->entities, sizeof(ecs_entity_record_t));
        *record = (ecs_entity_record_t){ 0, .table_row = row };
    }
    return ecs_entity(entity_id, generation);
}

static inline bool ecs_entity_index_is_alive(const ecs_entity_index_t *index, ecs_entity_t entity) {
    return ecs_entity_index_get_record(index, ecs_first(entity))->generation == ecs_second(entity);
}

static inline void ecs_entity_index_kill(ecs_entity_index_t *index, uint32_t entity_id) {
    ecs_entity_record_t *record = ecs_entity_index_get_record(index, entity_id);
    record->generation += 1;
    record->table_row = index->first_available;
    index->first_available = entity_id;
}

void ecs_entity_index_init(ecs_entity_index_t *index);
void ecs_entity_index_fini(ecs_entity_index_t *index);

#endif

#ifndef SIECS_STORAGE_OBSERVER_INDEX_H
#define SIECS_STORAGE_OBSERVER_INDEX_H
#ifndef SIECS_STORAGE_QUERY_INDEX_H
#define SIECS_STORAGE_QUERY_INDEX_H
#include <stdint.h>

typedef struct {
    uint64_t bloom;
    ecs_component_t *read;
    ecs_component_t *required;
    ecs_component_t *excluded;
    uint16_t read_count;
    uint16_t required_count;
    uint16_t excluded_count;
} ecs_query_t;

typedef struct ecs_query_cache_s {
    ecs_query_t query;
    ecs_vec_t table_ids; // uint16_t
    ecs_vec_t fields;    // void ** slots: &table->cls[col].data
} ecs_query_cache_t;

typedef struct {
    ecs_vec_t queries;
} ecs_query_index_t;

void ecs_query_index_init(ecs_query_index_t *index);
void ecs_query_index_fini(ecs_query_index_t *index);
uint16_t ecs_query_index_create(ecs_query_index_t *index, const ecs_query_desc_t *desc);
void ecs_query_index_update_matches(
    ecs_world_t *world,
    ecs_query_cache_t *query_cache
);
void ecs_query_index_add_table(
    ecs_query_index_t *index,
    const ecs_table_t *table,
    uint16_t table_id
);

// Reusable query helpers shared with the observer index.
void ecs_query_from_desc(const ecs_query_desc_t *desc, ecs_query_t *query);
void ecs_query_index_destroy(ecs_query_t *query);

static inline bool ecs_query_match_table(const ecs_query_t *query, const ecs_table_t *table) {
    if (ECS_LIKELY((query->bloom & table->bloom) != query->bloom)) {
        return false;
    }
    for (uint16_t i = 0; i < query->excluded_count; i++) {
        if (ecs_table_has(table, query->excluded[i])) {
            return false;
        }
    }
    for (uint16_t i = 0; i < query->required_count; i++) {
        if (!ecs_table_has(table, query->required[i])) {
            return false;
        }
    }
    for (uint16_t i = 0; i < query->read_count; i++) {
        if (!ecs_table_has(table, query->read[i])) {
            return false;
        }
    }
    return true;
}

#endif

#include <stdint.h>

typedef struct {
    ecs_event_t event;
    ecs_query_t query;
    ecs_observer_callback_t callback;
    uintptr_t user_data;
} ecs_observer_t;

typedef struct {
    ecs_vec_t observers;  // ecs_observer_t
    uint16_t event_count; // next free event id; starts past the builtin events
} ecs_observer_index_t;

void ecs_observer_index_init(ecs_observer_index_t *index);
void ecs_observer_index_fini(ecs_observer_index_t *index);

uint16_t ecs_observer_index_create(ecs_observer_index_t *index, const ecs_observer_desc_t *desc);

// Cache a freshly created observer onto every existing table it matches.
void ecs_observer_index_match_tables(
    ecs_observer_index_t *index,
    ecs_table_t *tables,
    uint16_t table_count,
    uint16_t observer_id
);

// Cache every existing observer that matches a freshly created table.
void ecs_observer_index_add_table(ecs_observer_index_t *index, ecs_table_t *table);

#endif

#ifndef SIECS_STORAGE_SYSTEM_INDEX_H
#define SIECS_STORAGE_SYSTEM_INDEX_H
#include <stdint.h>

typedef struct {
    const char *name;
    ecs_query_id_t qid;
    void (*callback)(ecs_iter_t *);
    ecs_phase_t phase;
    ecs_system_id_t after[4];
    bool enabled;
} ecs_system_t;

typedef struct {
    ecs_vec_t systems;
    ecs_vec_t phase_order[EcsPhaseCount];
    bool plan_dirty;
} ecs_system_index_t;

void ecs_system_index_init(ecs_system_index_t *index);
void ecs_system_index_fini(ecs_system_index_t *index);

ecs_system_id_t ecs_system_index_create(ecs_system_index_t *index, const ecs_system_t *system);
ecs_system_t *ecs_system_index_get(ecs_system_index_t *index, ecs_system_id_t system);
void ecs_system_index_build_plan(ecs_system_index_t *index);

#endif

typedef struct ecs_world_s {
    ecs_entity_index_t entity_index;
    ecs_component_index_t component_index;
    ecs_table_index_t table_index;
    ecs_query_index_t query_index;
    ecs_observer_index_t observer_index;
    ecs_system_index_t system_index;
} ecs_world_t;

typedef struct {
    ecs_entity_t target;
} RelationTarget;

typedef struct {
    ecs_vec_t entities;
} RelationSource;

#define ecs_get_record(world, entity)                                                              \
    ecs_vec_get_mut(&world->entity_index.entities, ecs_first(entity), ecs_entity_record_t)
#define ecs_get_table(world, tid) ecs_table_index_at(&world->table_index, tid)

static inline void ecs_emit(
    ecs_world_t *world,
    ecs_table_t *table,
    ecs_entity_t entity,
    ecs_event_t event,
    const void *trigger_data
) {
    if (table->observers_by_event.size <= event) {
        return;
    }
    const ecs_vec_t *list = ecs_vec_get(&table->observers_by_event, event, ecs_vec_t);
    uint32_t n = list->size;
    for (uint32_t i = 0; i < n; i++) {
        uint16_t oid = *ecs_vec_get(list, i, uint16_t);
        ecs_observer_t *obs =
            ecs_vec_get_mut(&world->observer_index.observers, oid, ecs_observer_t);
        ecs_observer_event_t observer_event = {
            .world = world,
            .entity = entity,
            .event = event,
            .user_data = obs->user_data,
            .trigger_data = trigger_data,
        };
        obs->callback(&observer_event);
    }
}

void ecs_bootstrap(ecs_world_t *world);
struct ecs_table_s *ecs_iter_table(ecs_iter_t *it);

#endif

#include <stdint.h>

void RelationOnSet(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t target_component,
    const void *ptr
) {
    const RelationTarget *target_data = ptr;
    ecs_component_t source_component = target_component + 1;

    const RelationTarget *old_target_data = ecs_get_cid(world, entity, target_component);

    ecs_assert_entity_valid(target_data->target);
    ecs_assert_is_alive(world, target_data->target);

    if (old_target_data->target == target_data->target) {
        return;
    }

    if (old_target_data->target) {
        RelationSource *source = ecs_get_cid(world, old_target_data->target, source_component);

        ecs_vec_remove_u64(&source->entities, entity);
        if (source->entities.size == 0) {
            ecs_remove_cid(world, old_target_data->target, source_component);
        }
    }

    if (ecs_has_cid(world, target_data->target, source_component)) {
        RelationSource *source_data = ecs_get_cid(world, target_data->target, source_component);
        ecs_vec_push_u64(&source_data->entities, entity);
    } else {
        RelationSource source_data = {};
        ecs_vec_init(&source_data.entities, sizeof(ecs_entity_t));
        ecs_vec_push_u64(&source_data.entities, entity);
        ecs_set_cid(world, target_data->target, source_component, &source_data);
    }
}

void RelationOnRemove(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t component,
    const void *ptr
) {
    const RelationTarget *target_data = ptr;
    ecs_component_t source_component = component + 1;
    RelationSource *target_source_data = ecs_get_cid(world, target_data->target, source_component);

    // Prevent recursive calls to RelationOnRemove when removing relation from child
    if (target_source_data->entities.size == UINT32_MAX) {
        return;
    }

    ecs_vec_remove_u64(&target_source_data->entities, entity);

    if (target_source_data->entities.size == 0) {
        ecs_remove_cid(world, target_data->target, source_component);
    }
}

void RelationSourceOnRemove(
    ecs_world_t *world,
    ecs_entity_t _entity,
    ecs_component_t component,
    const void *ptr
) {
    RelationSource *source_data = (void *)ptr;

    const ecs_entity_t *entities = source_data->entities.data;
    const uint32_t count = source_data->entities.size;

    // Prevent recursive calls to RelationOnRemove when removing relation from child
    source_data->entities.size = UINT32_MAX;
    for (uint32_t i = 0; i < count; i++) {
        ecs_remove_cid(world, entities[i], component - 1);
    }

    ecs_vec_fini(&source_data->entities);
}

ecs_component_t ecs_component_init(ecs_world_t *world, const ecs_component_desc_t *desc) {
    ecs_assert_not_null(world);

    if (desc->is_relation) {
        ecs_component_t component = ecs_component_index_create(
            &world->component_index,
            desc->name,
            desc->size,
            RelationOnSet,
            RelationOnRemove
        );
        ecs_component_index_create(
            &world->component_index,
            desc->source_name,
            sizeof(RelationSource),
            NULL,
            RelationSourceOnRemove
        );
        return component;
    } else {
        return ecs_component_index_create(
            &world->component_index,
            desc->name,
            desc->size,
            desc->on_set,
            desc->on_remove
        );
    }
}

ecs_entity_t ecs_new(ecs_world_t *world) {
    ecs_assert_not_null(world);
    ecs_table_t *table = ecs_get_table(world, 0);

    ecs_entity_t entity = ecs_entity_index_create(&world->entity_index, table->entity_count);
    ecs_table_add_entity(table, entity);

    return entity;
}

int ecs_is_alive(const ecs_world_t *world, ecs_entity_t entity) {
    return ecs_entity_index_is_alive(&world->entity_index, entity);
}

void ecs_kill(ecs_world_t *world, ecs_entity_t entity) {
    ecs_assert_not_null(world);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);

    for (int i = 0; i < table->type.count; i++) {
        const ecs_component_record_t *crec =
            ecs_component_index_get(&world->component_index, table->type.ids[i]);

        if (crec->on_remove) {
            crec->on_remove(
                world,
                entity,
                table->type.ids[i],
                ecs_table_get_component(table, table->type.ids[i], record->table_row)
            );
        }
    }

    // Remove from table
    ecs_entity_t moved = ecs_table_remove_entity(table, record->table_row);
    if (moved != entity) {
        ecs_get_record(world, moved)->table_row = record->table_row;
    }

    ecs_entity_index_kill(&world->entity_index, ecs_first(entity));
}

ecs_event_t ecs_event(ecs_world_t *world) { return world->observer_index.event_count++; }

uint32_t ecs_observer_init(ecs_world_t *world, const ecs_observer_desc_t *desc) {
    ecs_assert_not_null(world);
    ecs_assert(desc->callback != NULL, "Observer callback cannot be NULL");
    uint32_t oid = ecs_observer_index_create(&world->observer_index, desc);
    ecs_observer_index_match_tables(
        &world->observer_index,
        world->table_index.tables,
        world->table_index.table_count,
        oid
    );
    return oid;
}

void ecs_observer_trigger(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_event_t event,
    const void *trigger_data
) {
    ecs_assert_not_null(world);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);
    ecs_emit(world, table, entity, event, trigger_data);
}

#include <stdint.h>

uint32_t ecs_query_init(ecs_world_t *world, const ecs_query_desc_t *desc) {
    ecs_assert_not_null(world);
    uint32_t qid = ecs_query_index_create(&world->query_index, desc);
    ecs_query_index_update_matches(
        world,
        ecs_vec_get_mut(&world->query_index.queries, qid, ecs_query_cache_t)
    );
    return qid;
}

ecs_iter_t ecs_query_iter(ecs_world_t *world, uint16_t query_id) {
    ecs_assert_not_null(world);

    ecs_query_cache_t *cache =
        ecs_vec_get_mut(&world->query_index.queries, query_id, ecs_query_cache_t);
    return (ecs_iter_t){
        .world = world,
        .cache = cache,
        .table_idx = UINT16_MAX,
        .table_count = cache->table_ids.size,
        .count = 0,
    };
}

bool ecs_iter_next(ecs_iter_t *it) {
    uint16_t *tids = it->cache->table_ids.data;
    do {
        if (++it->table_idx >= it->table_count)
            return false;
        it->count = it->world->table_index.tables[tids[it->table_idx]].entity_count;
    } while (it->count == 0);
    it->ptrs = &((void ***)it->cache->fields.data)[it->table_idx * it->cache->query.read_count];
    return true;
}

ecs_table_t *ecs_iter_table(ecs_iter_t *it) {
    uint16_t tid = *ecs_vec_get_mut(&it->cache->table_ids, it->table_idx, uint16_t);
    return ecs_table_index_at(&it->world->table_index, tid);
}

void ecs_query_fini(ecs_world_t *world, ecs_query_id_t qid) {
    ecs_query_cache_t *cache = ecs_vec_get_mut(&world->query_index.queries, qid, ecs_query_cache_t);

    ecs_query_index_destroy(&cache->query);
    ecs_vec_fini(&cache->fields);
    ecs_vec_fini(&cache->table_ids);

    ecs_vec_remove_fast(&world->query_index.queries, qid, sizeof(ecs_query_cache_t));
}

#include <string.h>

ecs_system_id_t ecs_system_init(ecs_world_t *world, const ecs_system_desc_t *desc) {
    ecs_assert_not_null(world);
    ecs_assert_not_null(desc);
    ecs_assert_not_null(desc->callback);
    ecs_assert(desc->phase < EcsPhaseCount, "invalid system phase: %u\n", desc->phase);

    ecs_system_t sys = {
        .name = desc->name,
        .qid = ecs_query_init(world, &desc->query),
        .callback = desc->callback,
        .phase = desc->phase,
        .enabled = !desc->disabled,
    };

    memcpy(sys.after, desc->after, sizeof(ecs_system_id_t[4]));

    return ecs_system_index_create(&world->system_index, &sys);
}

void ecs_run_system(ecs_world_t *world, ecs_system_id_t system) {
    ecs_assert_not_null(world);

    ecs_system_t *sys = ecs_system_index_get(&world->system_index, system);
    if (!sys->enabled) {
        return;
    }

    ecs_iter_t it = ecs_query_iter(world, sys->qid);
    while (ecs_iter_next(&it)) {
        sys->callback(&it);
    }
}

void ecs_run_phase(ecs_world_t *world, ecs_phase_t phase) {
    ecs_assert_not_null(world);
    ecs_assert(phase < EcsPhaseCount, "invalid system phase: %u\n", phase);

    if (phase >= EcsPhaseCount) {
        return;
    }

    ecs_system_index_t *index = &world->system_index;
    if (index->plan_dirty) {
        ecs_system_index_build_plan(index);
    }

    ecs_vec_t *order = &index->phase_order[phase];
    for (uint32_t i = 0; i < order->size; i++) {
        ecs_system_id_t system = *ecs_vec_get(order, i, ecs_system_id_t);
        ecs_run_system(world, system);
    }
}

void ecs_progress(ecs_world_t *world) {
    ecs_assert_not_null(world);

    for (ecs_phase_t phase = 0; phase < EcsPhaseCount; phase++) {
        ecs_run_phase(world, phase);
    }
}

void ecs_enable_system(ecs_world_t *world, ecs_system_id_t system, bool enabled) {
    ecs_assert_not_null(world);

    ecs_system_t *sys = ecs_system_index_get(&world->system_index, system);
    if (sys->enabled == enabled) {
        return;
    }

    sys->enabled = enabled;
    world->system_index.plan_dirty = true;
}

#include <stdlib.h>

void ecs_table_init(
    ecs_table_t *table,
    ecs_type_t type,
    const ecs_component_index_t *component_index,
    uint16_t table_id
) {
    table->type = type;
    table->entity_capacity = 1;
    table->entity_count = 0;
    table->entities = malloc(sizeof(ecs_entity_t) * table->entity_capacity);
    table->cls = type.count == 0 ? NULL : malloc(sizeof(ecs_column_t) * type.count);
    table->bloom = ecs_type_bloom(&type);

    ecs_vec_init(&table->observers_by_event, sizeof(ecs_vec_t));
    ecs_id_map_init(&table->add_edge);

    for (uint16_t i = 0; i < type.count; i++) {
        ecs_component_record_t *rec = ecs_component_index_get_mut(component_index, type.ids[i]);
        ecs_vec_push_u16(&rec->tables, table_id);
        table->cls[i].size = rec->size;
        table->cls[i].data = rec->size != 0 ? malloc(rec->size * table->entity_capacity) : NULL;
        ecs_id_map_set(&table->add_edge, type.ids[i], i);
        table->cls[i].remove_edge = UINT16_MAX;
    }
}

static inline void ecs_table_grow(ecs_table_t *table) {
    uint64_t new_capacity = table->entity_capacity * (uint64_t)2;
    table->entities = realloc(table->entities, sizeof(ecs_entity_t) * new_capacity);
    for (uint16_t i = 0; i < table->type.count; i++) {
        if (table->cls[i].size != 0) {
            table->cls[i].data = realloc(table->cls[i].data, table->cls[i].size * new_capacity);
        }
    }
    table->entity_capacity = new_capacity;
}

uint32_t ecs_table_add_entity(ecs_table_t *table, ecs_entity_t entity) {
    if (ECS_UNLIKELY(table->entity_count >= table->entity_capacity)) {
        ecs_table_grow(table);
    }
    uint32_t row = table->entity_count++;
    table->entities[row] = entity;
    return row;
}

// if the entity is not the last one, the last entity will be moved to the removed entity's
// position, and the moved entity will be returned
ecs_entity_t ecs_table_remove_entity(ecs_table_t *table, uint32_t row) {
    ecs_entity_t removed_entity = table->entities[row];
    uint32_t last_row = table->entity_count - 1;
    if (row != last_row) {
        ecs_entity_t moved_entity = table->entities[last_row];
        table->entities[row] = moved_entity;
        for (uint16_t i = 0; i < table->type.count; i++) {
            if (table->cls[i].size != 0) {
                const void *src = (char *)table->cls[i].data + (table->cls[i].size * last_row);
                void *dst = (char *)table->cls[i].data + (table->cls[i].size * row);
                memcpy(dst, src, table->cls[i].size);
            }
        }
        table->entity_count -= 1;
        return moved_entity;
    }
    table->entity_count -= 1;
    return removed_entity;
}

void *ecs_table_get_component(ecs_table_t *table, ecs_component_t component_id, uint32_t row) {
    ecs_column_t *column = &table->cls[ecs_table_get_column_index(table, component_id)];
    return (uint8_t *)column->data + ((column->size) * row);
}

void ecs_table_add_observer(ecs_table_t *table, uint16_t event, uint16_t observer_id) {
    ecs_vec_ensure(&table->observers_by_event, event + 1, sizeof(ecs_vec_t));
    ecs_vec_t *list = ecs_vec_get_mut(&table->observers_by_event, event, ecs_vec_t);
    if (list->capacity == 0) {
        ecs_vec_init(list, sizeof(uint16_t));
    }
    ecs_vec_push_u16(list, observer_id);
}

void ecs_table_fini(ecs_table_t *table) {
    for (uint16_t i = 0; i < table->type.count; i++) {
        free(table->cls[i].data);
    }
    for (uint32_t e = 0; e < table->observers_by_event.size; e++) {
        ecs_vec_fini(ecs_vec_get_mut(&table->observers_by_event, e, ecs_vec_t));
    }
    ecs_vec_fini(&table->observers_by_event);
    ecs_id_map_fini(&table->add_edge);
    free(table->entities);
    free(table->cls);
    ecs_type_fini(&table->type);
}

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static inline void ecs_type_sort(ecs_type_t *type) {
    // insertion sort is efficient for nearly sorted arrays
    for (int i = 1; i < type->count; i++) {
        uint16_t key = type->ids[i];
        int j = i - 1;
        while (j >= 0 && type->ids[j] > key) {
            type->ids[j + 1] = type->ids[j];
            j--;
        }
        type->ids[j + 1] = key;
    }
}

ecs_type_t ecs_type_with_add(const ecs_type_t *type, uint16_t id) {
    ecs_type_t new_type = {
        .ids = malloc((type->count + 1) * sizeof(uint16_t)),
        .count = type->count + 1,
    };
    if (type->count > 0) {
        memcpy(new_type.ids, type->ids, type->count * sizeof(uint16_t));
    }
    new_type.ids[type->count] = id;
    ecs_type_sort(&new_type);
    return new_type;
}

ecs_type_t ecs_type_with_remove(const ecs_type_t *type, uint16_t id) {
    ecs_type_t new_type = {
        .ids = malloc((type->count - 1) * sizeof(uint16_t)),
        .count = type->count - 1,
    };
    int j = 0;
    for (int i = 0; i < type->count; i++) {
        if (type->ids[i] != id) {
            new_type.ids[j++] = type->ids[i];
        }
    }
    return new_type;
}

// returns the index of the id in the type, or -1 if not found
int ecs_type_find(const ecs_type_t *type, uint16_t id) {
    // binary search
    int left = 0, right = type->count - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (type->ids[mid] == id) {
            return mid;
        } else if (type->ids[mid] < id) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    return -1;
}

void ecs_type_fini(ecs_type_t *type) {
    if (type->ids) {
        free(type->ids);
        type->ids = NULL;
    }
}

uint64_t ecs_type_bloom(const ecs_type_t *type) {
    uint64_t filter = 0;

    for (uint16_t i = 0; i < type->count; i++) {
        filter |= (1ull << (type->ids[i] % 64));
    }

    return filter;
}

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

ecs_world_t *ecs_init() {
    ecs_world_t *world = malloc(sizeof(ecs_world_t));
    ecs_entity_index_init(&world->entity_index);
    ecs_component_index_init(&world->component_index);
    ecs_table_index_init(&world->table_index);
    ecs_query_index_init(&world->query_index);
    ecs_observer_index_init(&world->observer_index);
    ecs_system_index_init(&world->system_index);

    ecs_bootstrap(world);
    return world;
}

static inline void
copy_column(ecs_column_t *from, uint32_t from_row, ecs_column_t *to, uint32_t to_row) {
    memcpy(
        (uint8_t *)to->data + (from->size * to_row),
        (uint8_t *)from->data + (from->size * from_row),
        from->size
    );
}

static inline void migrate_entity_add(
    ecs_world_t *world,
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_id,
    ecs_component_t added_id
) {
    ecs_table_t *to_table = ecs_get_table(world, to_id);

    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    uint16_t k = ecs_table_get_column_index(to_table, added_id);
    if (to_table->cls[k].size != 0) {
        memset(
            (uint8_t *)to_table->cls[k].data + (to_table->cls[k].size * new_row),
            0,
            to_table->cls[k].size
        );
    }
    for (uint16_t i = 0; i < k; i++)
        copy_column(&from_table->cls[i], old_row, &to_table->cls[i], new_row);
    for (uint16_t i = k + 1; i < to_table->type.count; i++)
        copy_column(&from_table->cls[i - 1], old_row, &to_table->cls[i], new_row);

    ecs_entity_t moved = ecs_table_remove_entity(from_table, old_row);
    if (moved != entity)
        ecs_get_record(world, moved)->table_row = old_row;

    record->table_id = to_id;
    record->table_row = new_row;
}

static inline void migrate_entity_remove(
    ecs_world_t *world,
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_id,
    uint16_t col_idx
) {
    ecs_table_t *to_table = ecs_get_table(world, to_id);

    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    for (uint16_t i = 0; i < col_idx; i++)
        copy_column(&from_table->cls[i], old_row, &to_table->cls[i], new_row);
    for (uint16_t i = col_idx + 1; i < from_table->type.count; i++)
        copy_column(&from_table->cls[i], old_row, &to_table->cls[i - 1], new_row);

    ecs_entity_t moved = ecs_table_remove_entity(from_table, old_row);
    if (moved != entity)
        ecs_get_record(world, moved)->table_row = old_row;

    record->table_id = to_id;
    record->table_row = new_row;
}

void ecs_add_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    uint16_t from_id = record->table_id;
    ecs_table_t *table = ecs_get_table(world, from_id);

    if (ecs_table_has(table, cid)) {
        return;
    }

    const ecs_component_record_t *crec = ecs_component_index_get(&world->component_index, cid);
    for (uint32_t i = 0; i < crec->required_count; i++) {
        ecs_add_cid(world, entity, crec->required[i]);
    }

    record = ecs_get_record(world, entity);
    from_id = record->table_id;
    table = ecs_get_table(world, from_id);

    if (ecs_table_has(table, cid)) {
        return;
    }

    uint16_t new_table_id = ecs_table_get_add_edge(table, cid);

    if (new_table_id == UINT16_MAX) {
        ecs_type_t new_type = ecs_type_with_add(&table->type, cid);
        new_table_id = ecs_table_index_get_or_create(world, new_type);

        // Re-fetch: ecs_table_index_get_or_create may realloc the tables vec
        table = ecs_get_table(world, from_id);
        ecs_id_map_set(&table->add_edge, cid, new_table_id);
    } else if (
        ECS_UNLIKELY(new_table_id < table->type.count && table->type.ids[new_table_id] == cid)
    ) {
        return;
    }

    migrate_entity_add(world, record, entity, table, new_table_id, cid);

    ecs_table_t *new_table = ecs_get_table(world, new_table_id);
    const void *component_data = ecs_table_get_component(new_table, cid, record->table_row);
    ecs_emit(world, new_table, entity, OnAdd, component_data);
}

void ecs_remove_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    uint16_t from_id = record->table_id;
    ecs_table_t *table = ecs_get_table(world, from_id);

    int col_idx = ecs_table_get_column_index(table, cid);

    if (ECS_UNLIKELY(
            col_idx == UINT16_MAX || col_idx >= table->type.count || table->type.ids[col_idx] != cid
        )) {
        return;
    }

    uint16_t new_table_id = table->cls[col_idx].remove_edge;
    if (new_table_id == UINT16_MAX) {
        ecs_type_t new_type = ecs_type_with_remove(&table->type, cid);
        new_table_id = ecs_table_index_get_or_create(world, new_type);
        // Re-fetch: ecs_table_index_get_or_create may realloc the tables vec
        table = ecs_get_table(world, from_id);
        table->cls[col_idx].remove_edge = new_table_id;
    }

    void *removed_data = ecs_table_get_component(table, cid, record->table_row);

    const ecs_component_record_t *crec = ecs_component_index_get(&world->component_index, cid);
    if (crec->on_remove) {
        crec->on_remove(world, entity, cid, removed_data);
    }
    ecs_emit(world, table, entity, OnRemove, removed_data);

    migrate_entity_remove(world, record, entity, table, new_table_id, (uint16_t)col_idx);
}

void *ecs_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    const ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);
    return ecs_table_get_component(table, cid, record->table_row);
}

void *ecs_try_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    const ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);

    if (ecs_table_has(table, cid)) {
        return ecs_table_get_component(table, cid, record->table_row);
    }
    return NULL;
}

void ecs_set_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid, const void *data) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_add_cid(world, entity, cid);
    void *dst = ecs_get_cid(world, entity, cid);
    const ecs_component_record_t *crec = ecs_component_index_get(&world->component_index, cid);
    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);

    // on_set sees the new input data, while the table still stores the old data.
    // Hooks that need both can use ptr for new data and ecs_get_cid for old data.
    if (crec->on_set) {
        crec->on_set(world, entity, cid, data);
    }
    ecs_emit(world, table, entity, OnSet, data);
    memcpy(dst, data, crec->size);
}

bool ecs_has_cid(const ecs_world_t *world, ecs_entity_t entity, ecs_component_t id) {
    ecs_assert_not_null(world);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    uint16_t tid = ecs_get_record(world, entity)->table_id;
    return ecs_table_has(ecs_get_table(world, tid), id);
}

#ifndef NDEBUG
static bool ecs_component_requires(
    const ecs_world_t *world,
    ecs_component_t component,
    ecs_component_t require
) {
    const ecs_component_record_t *record =
        ecs_component_index_get(&world->component_index, component);

    for (uint32_t i = 0; i < record->required_count; i++) {
        ecs_component_t current = record->required[i];
        if (current == require || ecs_component_requires(world, current, require)) {
            return true;
        }
    }

    return false;
}
#endif

void ecs_with(ecs_world_t *world, ecs_component_t component, ecs_component_t require) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(component);
    ecs_assert_id_valid(require);
    ecs_assert(component != require, "component cannot require itself: %d\n", component);
    ecs_assert(
        !ecs_component_requires(world, require, component),
        "cyclic component requirement: %d requires %d\n",
        component,
        require
    );

    ecs_component_record_t *record =
        ecs_component_index_get_mut(&world->component_index, component);

    for (uint32_t i = 0; i < record->required_count; i++) {
        if (record->required[i] == require) {
            return;
        }
    }

    record->required =
        realloc(record->required, sizeof(ecs_component_t) * (record->required_count + 1));
    record->required[record->required_count++] = require;
}

void ecs_fini(ecs_world_t *world) {
    ecs_entity_index_fini(&world->entity_index);
    ecs_component_index_fini(&world->component_index);
    ecs_table_index_fini(&world->table_index);
    ecs_query_index_fini(&world->query_index);
    ecs_observer_index_fini(&world->observer_index);
    ecs_system_index_fini(&world->system_index);
    free(world);
}

#ifndef ECS_HTTP_SERVER
#define ECS_HTTP_SERVER

typedef struct {
    int sock;
} ecs_http_server_t;

#endif

#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>

typedef struct {
} ecs_http_request_t;

void ecs_http_server_init(ecs_http_server_t *server) {
    server->sock = socket(AF_INET, SOCK_STREAM, 0);
}

static inline void
ecs_http_server_parse_request(ecs_http_request_t *request, const char *request_str) {}

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void ecs_id_map_init(ecs_id_map_t *map) {
    map->capacity = 1;
    map->ids = malloc(sizeof(uint16_t));
    *map->ids = UINT16_MAX;
}

void ecs_id_map_fini(ecs_id_map_t *map) { free(map->ids); }

void ecs_id_map_ensure(ecs_id_map_t *map, uint16_t id) {
    if (ECS_UNLIKELY(id >= map->capacity)) {
        uint16_t new_cap = map->capacity;
        while (new_cap <= id)
            new_cap *= 2;
        map->ids = realloc(map->ids, sizeof(uint16_t) * new_cap);
        memset(map->ids + map->capacity, 0xFF, sizeof(uint16_t) * (new_cap - map->capacity));
        map->capacity = new_cap;
    }
}

#ifndef NDEBUG
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ECS_MAP_LOAD_NUM 7
#define ECS_MAP_LOAD_DEN 10

static size_t ecs_next_pow2(size_t x) {
    size_t p = 16;
    while (p < x)
        p <<= 1;
    return p;
}

static inline uint64_t ecs_map_hash_cstr(const char *s) {
    uint64_t h = 1469598103934665603ull;
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 1099511628211ull;
    }

    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdull;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ull;
    h ^= h >> 33;

    return h ? h : 1;
}

void ecs_map_init(ecs_map_t *m, size_t initial_capacity) {
    size_t cap = ecs_next_pow2(initial_capacity);
    m->slots = (ecs_map_slot_t *)calloc(cap, sizeof(ecs_map_slot_t));

    m->cap = cap;
    m->len = 0;
}

void ecs_map_fini(ecs_map_t *m) {
    if (!m)
        return;

    if (m->slots) {
        free(m->slots);
    }

    m->slots = NULL;
    m->cap = 0;
    m->len = 0;
}

static bool ecs_map_insert_raw(
    ecs_map_slot_t *slots,
    size_t cap,
    const char *key,
    uint64_t hash,
    uint32_t value
) {
    size_t mask = cap - 1;
    size_t i = (size_t)hash & mask;

    for (;;) {
        ecs_map_slot_t *s = &slots[i];

        if (!s->key) {
            s->key = key;
            s->hash = hash;
            s->value = value;
            return true;
        }

        if (s->hash == hash && (s->key == key || strcmp(s->key, key) == 0)) {
            s->value = value;
            return false;
        }

        i = (i + 1) & mask;
    }
}

static void ecs_map_grow(ecs_map_t *m) {
    size_t new_cap = m->cap ? m->cap * 2 : 16;
    ecs_map_slot_t *new_slots = (ecs_map_slot_t *)calloc(new_cap, sizeof(ecs_map_slot_t));

    for (size_t i = 0; i < m->cap; i++) {
        const ecs_map_slot_t *s = &m->slots[i];
        if (s->key) {
            ecs_map_insert_raw(new_slots, new_cap, s->key, s->hash, s->value);
        }
    }

    free(m->slots);
    m->slots = new_slots;
    m->cap = new_cap;
}

void ecs_map_set(ecs_map_t *m, const char *key, uint32_t value) {
    if (!m->slots) {
        ecs_map_init(m, 16);
    }

    if ((m->len + 1) * ECS_MAP_LOAD_DEN > m->cap * ECS_MAP_LOAD_NUM) {
        ecs_map_grow(m);
    }

    uint64_t hash = ecs_map_hash_cstr(key);
    bool inserted = ecs_map_insert_raw(m->slots, m->cap, key, hash, value);

    if (inserted) {
        m->len++;
    }
}

uint32_t ecs_map_get(const ecs_map_t *m, const char *key) {
    if (!m->slots)
        return UINT32_MAX;

    uint64_t hash = ecs_map_hash_cstr(key);
    size_t mask = m->cap - 1;
    size_t i = (size_t)hash & mask;

    for (;;) {
        const ecs_map_slot_t *s = &m->slots[i];

        if (!s->key)
            return UINT32_MAX;

        if (s->hash == hash && (s->key == key || strcmp(s->key, key) == 0)) {
            return s->value;
        }

        i = (i + 1) & mask;
    }
}

bool ecs_map_has(const ecs_map_t *m, const char *key) { return ecs_map_get(m, key) != UINT32_MAX; }
#endif

#ifndef ECS_STRING_H
#define ECS_STRING_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char *data; // null terminated string
    uint32_t len;
    uint32_t capacity;
} ecs_str_t;

void ecs_str_init(ecs_str_t *str);
void ecs_str_fini(ecs_str_t *str);
ecs_str_t ecs_str_new();
ecs_str_t ecs_str_with_capacity(uint32_t capacity);

void ecs_str_reserve(ecs_str_t *str, uint32_t capacity);
void ecs_str_resize(ecs_str_t *str, uint32_t len);
ecs_str_t ecs_str_from_cstr(const char *cstr);
ecs_str_t ecs_str_clone(const ecs_str_t *str);

const char *ecs_str_cstr(const ecs_str_t *str);
char ecs_str_at(const ecs_str_t *str, uint32_t index);

void ecs_str_char_append(ecs_str_t *dst, char src);
void ecs_str_str_append(ecs_str_t *dst, const ecs_str_t *src);
void ecs_str_insert(ecs_str_t *str, uint32_t pos, char c);
void ecs_str_remove(ecs_str_t *str, uint32_t pos);
void ecs_str_pop_back(ecs_str_t *str);

void ecs_str_trim(ecs_str_t *str);

bool ecs_str_starts_with(const ecs_str_t *str, const ecs_str_t *prefix);
bool ecs_str_ends_with(const ecs_str_t *str, const ecs_str_t *suffix);
bool ecs_str_cmp(const ecs_str_t *a, const ecs_str_t *b);

#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

void ecs_str_init(ecs_str_t *str) {
    str->data = NULL;
    str->len = 0;
    str->capacity = 0;
}

void ecs_str_fini(ecs_str_t *str) {
    if (str->data) {
        free(str->data);
    }
    ecs_str_init(str);
}

ecs_str_t ecs_str_new() {
    ecs_str_t str;
    ecs_str_init(&str);
    return str;
}

ecs_str_t ecs_str_with_capacity(uint32_t capacity) {
    ecs_str_t str;
    str.len = 0;
    str.capacity = capacity;
    if (capacity > 0) {
        str.data = malloc(capacity + 1);
        str.data[0] = '\0';
    } else {
        str.data = NULL;
    }
    return str;
}

void ecs_str_reserve(ecs_str_t *str, uint32_t capacity) {
    if (capacity > str->capacity) {
        str->data = realloc(str->data, capacity + 1);
        str->capacity = capacity;
        if (str->len == 0 && str->data) {
            str->data[0] = '\0';
        }
    }
}

void ecs_str_resize(ecs_str_t *str, uint32_t len) {
    ecs_str_reserve(str, len);
    if (str->data) {
        str->data[len] = '\0';
    }
    str->len = len;
}

ecs_str_t ecs_str_from_cstr(const char *cstr) {
    if (!cstr)
        return ecs_str_new();
    uint32_t len = (uint32_t)strlen(cstr);
    ecs_str_t str = ecs_str_with_capacity(len);
    if (len > 0) {
        memcpy(str.data, cstr, len + 1);
        str.len = len;
    }
    return str;
}

ecs_str_t ecs_str_clone(const ecs_str_t *str) {
    if (!str || !str->data)
        return ecs_str_new();
    ecs_str_t new_str = ecs_str_with_capacity(str->len);
    if (str->len > 0) {
        memcpy(new_str.data, str->data, str->len + 1);
        new_str.len = str->len;
    }
    return new_str;
}

const char *ecs_str_cstr(const ecs_str_t *str) { return str->data ? str->data : ""; }

char ecs_str_at(const ecs_str_t *str, uint32_t index) {
    ecs_assert(index < str->len, "index out of bounds: %d (len: %d)", index, str->len);
    return str->data[index];
}

void ecs_str_char_append(ecs_str_t *dst, char src) {
    if (dst->len + 1 > dst->capacity) {
        uint32_t new_cap = dst->capacity == 0 ? 8 : dst->capacity * 2;
        ecs_str_reserve(dst, new_cap);
    }
    dst->data[dst->len++] = src;
    dst->data[dst->len] = '\0';
}

void ecs_str_str_append(ecs_str_t *dst, const ecs_str_t *src) {
    if (!src || src->len == 0)
        return;
    uint32_t required = dst->len + src->len;
    if (required > dst->capacity) {
        ecs_str_reserve(dst, required);
    }
    memcpy(dst->data + dst->len, src->data, src->len);
    dst->len = required;
    dst->data[dst->len] = '\0';
}

void ecs_str_insert(ecs_str_t *str, uint32_t pos, char c) {
    ecs_assert(pos <= str->len, "pos out of bounds: %d (len: %d)", pos, str->len);
    if (str->len + 1 > str->capacity) {
        uint32_t new_cap = str->capacity == 0 ? 8 : str->capacity * 2;
        ecs_str_reserve(str, new_cap);
    }
    memmove(str->data + pos + 1, str->data + pos, str->len - pos + 1);
    str->data[pos] = c;
    str->len++;
}

void ecs_str_remove(ecs_str_t *str, uint32_t pos) {
    ecs_assert(pos < str->len, "pos out of bounds: %d (len: %d)", pos, str->len);
    memmove(str->data + pos, str->data + pos + 1, str->len - pos);
    str->len--;
}

void ecs_str_pop_back(ecs_str_t *str) {
    if (str->len > 0) {
        str->len--;
        str->data[str->len] = '\0';
    }
}

void ecs_str_trim(ecs_str_t *str) {
    if (str->len == 0)
        return;
    uint32_t start = 0;
    while (start < str->len && isspace((unsigned char)str->data[start])) {
        start++;
    }
    if (start == str->len) {
        str->len = 0;
        str->data[0] = '\0';
        return;
    }
    uint32_t end = str->len - 1;
    while (end > start && isspace((unsigned char)str->data[end])) {
        end--;
    }
    uint32_t new_len = end - start + 1;
    if (start > 0) {
        memmove(str->data, str->data + start, new_len);
    }
    str->len = new_len;
    str->data[str->len] = '\0';
}

bool ecs_str_starts_with(const ecs_str_t *str, const ecs_str_t *prefix) {
    if (prefix->len > str->len)
        return false;
    return memcmp(str->data, prefix->data, prefix->len) == 0;
}

bool ecs_str_ends_with(const ecs_str_t *str, const ecs_str_t *suffix) {
    if (suffix->len > str->len)
        return false;
    return memcmp(str->data + (str->len - suffix->len), suffix->data, suffix->len) == 0;
}

bool ecs_str_cmp(const ecs_str_t *a, const ecs_str_t *b) {
    if (a->len != b->len)
        return false;
    if (a->len == 0)
        return true;
    return memcmp(a->data, b->data, a->len) == 0;
}

#include <stdlib.h>
#include <string.h>

void ecs_vec_init(ecs_vec_t *vec, uint32_t element_size) {
    vec->data = malloc(element_size); // Start with 1 elements
    vec->size = 0;
    vec->capacity = 1;
}

void ecs_vec_fini(ecs_vec_t *vec) { free(vec->data); }

void ecs_vec_grow(ecs_vec_t *vec, uint32_t element_size) {
    vec->capacity *= 2;
    vec->data = realloc(vec->data, element_size * vec->capacity);
}

void ecs_vec_resize_max(ecs_vec_t *vec, uint32_t new_capacity, uint32_t element_size) {
    if (new_capacity > vec->capacity) {
        vec->data = realloc(vec->data, element_size * new_capacity);
        memset(
            (uint8_t *)vec->data + (element_size * vec->capacity),
            0xFF,
            element_size * (new_capacity - vec->capacity)
        );
        vec->capacity = new_capacity;
    }
    if (new_capacity < vec->size) {
        vec->size = new_capacity;
    }
}

void ecs_vec_push(ecs_vec_t *vec, const void *element, const uint32_t element_size) {
    if (ECS_UNLIKELY(vec->size >= vec->capacity)) {
        ecs_vec_grow(vec, element_size);
    }
    memcpy((uint8_t *)vec->data + (vec->size * element_size), element, element_size);
    vec->size++;
}

void ecs_vec_ensure(ecs_vec_t *vec, uint32_t count, const uint32_t element_size) {
    if (count <= vec->size)
        return;
    while (vec->capacity < count)
        ecs_vec_grow(vec, element_size);
    memset((uint8_t *)vec->data + vec->size * element_size, 0, (count - vec->size) * element_size);
    vec->size = count;
}

void ecs_vec_remove_fast(ecs_vec_t *vec, uint32_t index, const uint32_t element_size) {
    if (index < vec->size - 1) {
        void *dst = (uint8_t *)vec->data + (index * element_size);
        const void *src = (uint8_t *)vec->data + ((vec->size - 1) * element_size);
        memcpy(dst, src, element_size);
    }
    vec->size--;
}

bool ecs_vec_contains_u64(const ecs_vec_t *vec, const uint64_t value) {
    ecs_vec_iter(vec, uint64_t, current, {
        if (*current == value) {
            return true;
        }
    });
    return false;
}

static inline void ecs_vec_remove_fast_u64(ecs_vec_t *vec, uint32_t index) {
    if (index < vec->size - 1) {
        uint64_t *data = vec->data;
        data[index] = data[vec->size - 1];
    }
    vec->size--;
}

void ecs_vec_remove_u64(ecs_vec_t *vec, uint64_t value) {
    ecs_vec_iter(vec, uint64_t, current, {
        if (*current == value) {
            ecs_vec_remove_fast_u64(vec, i);
            return;
        }
    });
}

#ifndef ECS_LEXER_H
#define ECS_LEXER_H

#include "datastructure/vec.h"

typedef enum {
    EcsTokEnd = '\0',
    EcsTokUnknown,
    EcsTokScopeOpen = '{',
    EcsTokScopeClose = '}',
    EcsTokParenOpen = '(',
    EcsTokParenClose = ')',
    EcsTokBracketOpen = '[',
    EcsTokBracketClose = ']',
    EcsTokMember = '.',
    EcsTokComma = ',',
    EcsTokSemiColon = ';',
    EcsTokColon = ':',
    EcsTokAssign = '=',
    EcsTokAdd = '+',
    EcsTokSub = '-',
    EcsTokMul = '*',
    EcsTokDiv = '/',
    EcsTokMod = '%',
    EcsTokBitwiseOr = '|',
    EcsTokBitwiseAnd = '&',
    EcsTokNot = '!',
    EcsTokOptional = '?',
    EcsTokEq = 100,              // ==
    EcsTokNeq = 101,             // !=
    EcsTokGt = 102,              // >
    EcsTokGtEq = 103,            // >=
    EcsTokLt = 104,              // <
    EcsTokLtEq = 105,            // <=
    EcsTokAnd = 106,             // &&
    EcsTokOr = 107,              // ||
    EcsTokMatch = 108,           // ~=
    EcsTokRange = 109,           // ..
    EcsTokShiftLeft = 110,       // <<
    EcsTokShiftRight = 111,      // >>
    EcsTokIdentifier = 112,      // identifier
    EcsTokFunction = 113,        // function
    EcsTokString = 114,          // string literal
    EcsTokNumber = 115,          // number literal
    EcsTokKeywordModule = 116,   // module
    EcsTokKeywordUsing = 117,    // using
    EcsTokKeywordWith = 118,     // with
    EcsTokKeywordIf = 119,       // if
    EcsTokKeywordFor = 120,      // for
    EcsTokKeywordIn = 121,       // in
    EcsTokKeywordElse = 122,     // else
    EcsTokKeywordTemplate = 130, // template
    EcsTokKeywordProp = 131,     // prop
    EcsTokKeywordConst = 132,    // const
    EcsTokKeywordMatch = 133,    // match
    EcsTokKeywordNew = 134,      // new
    EcsTokKeywordExport = 135,   // export
    EcsTokKeywordInclude = 138,  // include
    EcsTokKeywordFn = 139,       // fn
    EcsTokArrow = 140,           // =>
    EcsTokAddAssign = 136,       // +=
    EcsTokMulAssign = 137,       // *=
} ecs_token_type_t;

typedef struct {
    const char *data;
    uint32_t len;
} ecs_token_slice_t;

typedef struct {
    ecs_token_type_t type;
    union {
        ecs_token_slice_t str;
        double number;
        char character;
    } data;
} ecs_token_t;

void ecs_lexer_lex(const char *str, ecs_vec_t *tokens); // tokens = ecs_token_t

#endif

#include "parsing/scanner.h"

#include <string.h>

static inline void ecs_lexer_push(ecs_vec_t *tokens, ecs_token_type_t type) {
    ecs_token_t *token = ecs_vec_push_empty(tokens, sizeof(ecs_token_t));
    token->type = type;
}

static inline void ecs_lexer_push_char(ecs_vec_t *tokens, ecs_token_type_t type, char value) {
    ecs_token_t *token = ecs_vec_push_empty(tokens, sizeof(ecs_token_t));
    token->type = type;
    token->data.character = value;
}

static inline void
ecs_lexer_push_slice(ecs_vec_t *tokens, ecs_token_type_t type, const char *data, uint32_t len) {
    ecs_token_t *token = ecs_vec_push_empty(tokens, sizeof(ecs_token_t));
    token->type = type;
    token->data.str = (ecs_token_slice_t){ data, len };
}

static inline void ecs_lexer_push_number(ecs_vec_t *tokens, double value) {
    ecs_token_t *token = ecs_vec_push_empty(tokens, sizeof(ecs_token_t));
    token->type = EcsTokNumber;
    token->data.number = value;
}

static inline bool ecs_lexer_slice_eq(const char *data, uint32_t len, const char *keyword) {
    uint32_t keyword_len = (uint32_t)strlen(keyword);
    return len == keyword_len && memcmp(data, keyword, len) == 0;
}

static ecs_token_type_t ecs_lexer_keyword_type(const char *data, uint32_t len) {
    switch (len) {
    case 2:
        if (ecs_lexer_slice_eq(data, len, "if"))
            return EcsTokKeywordIf;
        if (ecs_lexer_slice_eq(data, len, "in"))
            return EcsTokKeywordIn;
        if (ecs_lexer_slice_eq(data, len, "fn"))
            return EcsTokKeywordFn;
        break;
    case 3:
        if (ecs_lexer_slice_eq(data, len, "for"))
            return EcsTokKeywordFor;
        if (ecs_lexer_slice_eq(data, len, "new"))
            return EcsTokKeywordNew;
        break;
    case 4:
        if (ecs_lexer_slice_eq(data, len, "with"))
            return EcsTokKeywordWith;
        if (ecs_lexer_slice_eq(data, len, "else"))
            return EcsTokKeywordElse;
        if (ecs_lexer_slice_eq(data, len, "prop"))
            return EcsTokKeywordProp;
        break;
    case 5:
        if (ecs_lexer_slice_eq(data, len, "using"))
            return EcsTokKeywordUsing;
        if (ecs_lexer_slice_eq(data, len, "const"))
            return EcsTokKeywordConst;
        if (ecs_lexer_slice_eq(data, len, "match"))
            return EcsTokKeywordMatch;
        break;
    case 6:
        if (ecs_lexer_slice_eq(data, len, "module"))
            return EcsTokKeywordModule;
        if (ecs_lexer_slice_eq(data, len, "export"))
            return EcsTokKeywordExport;
        break;
    case 7:
        if (ecs_lexer_slice_eq(data, len, "include"))
            return EcsTokKeywordInclude;
        break;
    case 8:
        if (ecs_lexer_slice_eq(data, len, "function"))
            return EcsTokFunction;
        if (ecs_lexer_slice_eq(data, len, "template"))
            return EcsTokKeywordTemplate;
        break;
    }
    return EcsTokIdentifier;
}

static void ecs_lexer_lex_identifier(ecs_scanner_t *scanner, ecs_vec_t *tokens) {
    const char *start = ecs_scanner_current_ptr(scanner);
    uint32_t start_pos = scanner->pos;

    ecs_scanner_advance(scanner);
    while (!ecs_scanner_is_done(scanner) && ecs_is_identifier_part(ecs_scanner_peek(scanner))) {
        ecs_scanner_advance(scanner);
    }

    uint32_t len = scanner->pos - start_pos;
    ecs_lexer_push_slice(tokens, ecs_lexer_keyword_type(start, len), start, len);
}

static void ecs_lexer_lex_number(ecs_scanner_t *scanner, ecs_vec_t *tokens) {
    double value = 0.0;

    while (!ecs_scanner_is_done(scanner) && isdigit(ecs_scanner_peek(scanner))) {
        value = value * 10.0 + (double)(ecs_scanner_peek(scanner) - '0');
        ecs_scanner_advance(scanner);
    }

    if (!ecs_scanner_is_done(scanner) && ecs_scanner_peek(scanner) == '.' &&
        isdigit(ecs_scanner_peek_next(scanner))) {
        double place = 0.1;
        ecs_scanner_advance(scanner);
        while (!ecs_scanner_is_done(scanner) && isdigit(ecs_scanner_peek(scanner))) {
            value += (double)(ecs_scanner_peek(scanner) - '0') * place;
            place *= 0.1;
            ecs_scanner_advance(scanner);
        }
    }

    if (!ecs_scanner_is_done(scanner) &&
        (ecs_scanner_peek(scanner) == 'e' || ecs_scanner_peek(scanner) == 'E')) {
        uint32_t pos = scanner->pos + 1;
        bool negative = false;

        if (pos < scanner->len && (scanner->str[pos] == '+' || scanner->str[pos] == '-')) {
            negative = scanner->str[pos] == '-';
            pos++;
        }

        if (pos < scanner->len && isdigit(scanner->str[pos])) {
            uint32_t exponent = 0;
            double scale = 1.0;

            scanner->pos = pos;
            while (!ecs_scanner_is_done(scanner) && isdigit(ecs_scanner_peek(scanner))) {
                exponent = exponent * 10 + (uint32_t)(ecs_scanner_peek(scanner) - '0');
                ecs_scanner_advance(scanner);
            }

            while (exponent-- > 0) {
                scale *= 10.0;
            }

            value = negative ? value / scale : value * scale;
        }
    }

    ecs_lexer_push_number(tokens, value);
}

static void ecs_lexer_lex_string(ecs_scanner_t *scanner, ecs_vec_t *tokens) {
    ecs_scanner_advance(scanner);

    const char *start = ecs_scanner_current_ptr(scanner);
    uint32_t start_pos = scanner->pos;

    while (!ecs_scanner_is_done(scanner) && ecs_scanner_peek(scanner) != '"') {
        if (ecs_scanner_peek(scanner) == '\\' && ecs_scanner_peek_next(scanner) != '\0') {
            ecs_scanner_advance_n(scanner, 2);
        } else {
            ecs_scanner_advance(scanner);
        }
    }

    uint32_t len = scanner->pos - start_pos;
    if (!ecs_scanner_is_done(scanner)) {
        ecs_scanner_advance(scanner);
    }

    ecs_lexer_push_slice(tokens, EcsTokString, start, len);
}

static bool ecs_lexer_try_two_char(
    ecs_scanner_t *scanner,
    ecs_vec_t *tokens,
    char next,
    ecs_token_type_t type
) {
    if (ecs_scanner_peek_next(scanner) != next) {
        return false;
    }
    ecs_lexer_push(tokens, type);
    ecs_scanner_advance_n(scanner, 2);
    return true;
}

void ecs_lexer_lex(const char *str, ecs_vec_t *tokens) {
    ecs_scanner_t scanner;
    ecs_scanner_init(&scanner, str);

    while (!ecs_scanner_is_done(&scanner)) {
        ecs_scanner_skip_while(&scanner, isspace);
        if (ecs_scanner_is_done(&scanner)) {
            break;
        }

        char c = ecs_scanner_peek(&scanner);

        if (ecs_is_identifier_start(c)) {
            ecs_lexer_lex_identifier(&scanner, tokens);
            continue;
        }

        if (isdigit(c)) {
            ecs_lexer_lex_number(&scanner, tokens);
            continue;
        }

        if (c == '"') {
            ecs_lexer_lex_string(&scanner, tokens);
            continue;
        }

        switch (c) {
        case '{':
        case '}':
        case '(':
        case ')':
        case '[':
        case ']':
        case ',':
        case ';':
        case ':':
        case '/':
        case '%':
        case '?':
            ecs_lexer_push(tokens, (ecs_token_type_t)c);
            ecs_scanner_advance(&scanner);
            break;
        case '.':
            if (!ecs_lexer_try_two_char(&scanner, tokens, '.', EcsTokRange)) {
                ecs_lexer_push(tokens, EcsTokMember);
                ecs_scanner_advance(&scanner);
            }
            break;
        case '=':
            if (!ecs_lexer_try_two_char(&scanner, tokens, '=', EcsTokEq) &&
                !ecs_lexer_try_two_char(&scanner, tokens, '>', EcsTokArrow)) {
                ecs_lexer_push(tokens, EcsTokAssign);
                ecs_scanner_advance(&scanner);
            }
            break;
        case '+':
            if (!ecs_lexer_try_two_char(&scanner, tokens, '=', EcsTokAddAssign)) {
                ecs_lexer_push(tokens, EcsTokAdd);
                ecs_scanner_advance(&scanner);
            }
            break;
        case '-':
            ecs_lexer_push(tokens, EcsTokSub);
            ecs_scanner_advance(&scanner);
            break;
        case '*':
            if (!ecs_lexer_try_two_char(&scanner, tokens, '=', EcsTokMulAssign)) {
                ecs_lexer_push(tokens, EcsTokMul);
                ecs_scanner_advance(&scanner);
            }
            break;
        case '|':
            if (!ecs_lexer_try_two_char(&scanner, tokens, '|', EcsTokOr)) {
                ecs_lexer_push(tokens, EcsTokBitwiseOr);
                ecs_scanner_advance(&scanner);
            }
            break;
        case '&':
            if (!ecs_lexer_try_two_char(&scanner, tokens, '&', EcsTokAnd)) {
                ecs_lexer_push(tokens, EcsTokBitwiseAnd);
                ecs_scanner_advance(&scanner);
            }
            break;
        case '!':
            if (!ecs_lexer_try_two_char(&scanner, tokens, '=', EcsTokNeq)) {
                ecs_lexer_push(tokens, EcsTokNot);
                ecs_scanner_advance(&scanner);
            }
            break;
        case '~':
            if (!ecs_lexer_try_two_char(&scanner, tokens, '=', EcsTokMatch)) {
                ecs_lexer_push_char(tokens, EcsTokUnknown, c);
                ecs_scanner_advance(&scanner);
            }
            break;
        case '<':
            if (!ecs_lexer_try_two_char(&scanner, tokens, '=', EcsTokLtEq) &&
                !ecs_lexer_try_two_char(&scanner, tokens, '<', EcsTokShiftLeft)) {
                ecs_lexer_push(tokens, EcsTokLt);
                ecs_scanner_advance(&scanner);
            }
            break;
        case '>':
            if (!ecs_lexer_try_two_char(&scanner, tokens, '=', EcsTokGtEq) &&
                !ecs_lexer_try_two_char(&scanner, tokens, '>', EcsTokShiftRight)) {
                ecs_lexer_push(tokens, EcsTokGt);
                ecs_scanner_advance(&scanner);
            }
            break;
        default:
            ecs_lexer_push_char(tokens, EcsTokUnknown, c);
            ecs_scanner_advance(&scanner);
            break;
        }
    }

    ecs_lexer_push(tokens, EcsTokEnd);
}

#ifndef ECS_PARSING_SCANNER_H
#define ECS_PARSING_SCANNER_H

#include "datastructure/string.h"
#include "datastructure/vec.h"
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    const char *str;
    uint32_t pos;
    uint32_t len;
} ecs_scanner_t;

void ecs_scanner_init(ecs_scanner_t *scanner, const char *str);

static inline bool ecs_scanner_is_done(const ecs_scanner_t *scanner) {
    return scanner->pos >= scanner->len;
}

static inline void ecs_scanner_advance(ecs_scanner_t *scanner) { scanner->pos += 1; }

static inline char ecs_scanner_peek(const ecs_scanner_t *scanner) {
    return scanner->str[scanner->pos];
}

static inline char ecs_scanner_pop(ecs_scanner_t *scanner) {
    char letter = ecs_scanner_peek(scanner);
    ecs_scanner_advance(scanner);
    return letter;
}

typedef int (*ecs_scanner_cmp_t)(int);

static inline void ecs_scanner_skip_while(ecs_scanner_t *scanner, const ecs_scanner_cmp_t cmp) {
    while (!ecs_scanner_is_done(scanner) && cmp(ecs_scanner_peek(scanner))) {
        ecs_scanner_advance(scanner);
    }
}

static inline void ecs_scanner_skip_whitespace(ecs_scanner_t *scanner) {
    ecs_scanner_skip_while(scanner, isblank);
}

static inline ecs_str_t
ecs_scanner_take_while(ecs_scanner_t *scanner, const ecs_scanner_cmp_t cmp) {
    ecs_str_t str = ecs_str_new();

    while (!ecs_scanner_is_done(scanner) && cmp(ecs_scanner_peek(scanner))) {
        ecs_str_char_append(&str, ecs_scanner_pop(scanner));
    }

    return str;
}

static inline bool ecs_is_identifier_start(int c) { return isalpha(c) || c == '_'; }

static inline bool ecs_is_identifier_part(int c) { return isalnum(c) || c == '_'; }

static inline ecs_str_t ecs_scanner_take_identifier(ecs_scanner_t *scanner) {
    ecs_str_t str = ecs_str_new();

    if (ecs_scanner_is_done(scanner) || !ecs_is_identifier_start(ecs_scanner_peek(scanner))) {
        return str;
    }

    ecs_str_char_append(&str, ecs_scanner_pop(scanner));

    while (!ecs_scanner_is_done(scanner) && ecs_is_identifier_part(ecs_scanner_peek(scanner))) {
        ecs_str_char_append(&str, ecs_scanner_pop(scanner));
    }

    return str;
}

static inline char ecs_scanner_peek_next(const ecs_scanner_t *scanner) {
    if (scanner->pos + 1 >= scanner->len) {
        return '\0';
    }
    return scanner->str[scanner->pos + 1];
}

static inline bool ecs_scanner_match(ecs_scanner_t *scanner, char expected) {
    if (ecs_scanner_is_done(scanner)) {
        return false;
    }
    return scanner->str[scanner->pos] == expected;
}

static inline const char *ecs_scanner_current_ptr(const ecs_scanner_t *scanner) {
    return scanner->str + scanner->pos;
}

static inline void ecs_scanner_advance_n(ecs_scanner_t *scanner, uint64_t count) {
    scanner->pos += count;
}

#endif

#include <string.h>

void ecs_scanner_init(ecs_scanner_t *scanner, const char *str) {
    scanner->str = str;
    scanner->pos = 0;
    scanner->len = (uint32_t)strlen(str);
}

#include <stdlib.h>

ecs_component_t ecs_component_index_create(
    ecs_component_index_t *index,
    const char *name,
    uint64_t size,
    ecs_component_hook_t on_set,
    ecs_component_hook_t on_remove
) {
    ecs_component_record_t record = {
        .name = name,
        .required = NULL,
        .required_count = 0,
        .size = size,
        .on_set = on_set,
        .on_remove = on_remove,
        .tables = { 0 },
    };
    ecs_vec_init(&record.tables, sizeof(uint16_t));

    ecs_vec_push(&index->components, &record, sizeof(ecs_component_record_t));
#ifndef NDEBUG
    if (name) {
        ecs_map_set(&index->component_name_map, name, index->components.size - 1);
    }
#endif
    return index->components.size - 1;
}

void ecs_component_index_init(ecs_component_index_t *index) {
    ecs_vec_init(&index->components, sizeof(ecs_component_record_t));
#ifndef NDEBUG
    ecs_map_init(&index->component_name_map, 16);
#endif
}

void ecs_component_index_fini(ecs_component_index_t *index) {
    ecs_component_record_t *records = index->components.data;

    for (uint32_t i = 0; i < index->components.size; i++) {
        free(records[i].required);
        ecs_vec_fini(&records[i].tables);
    }
    ecs_vec_fini(&index->components);

#ifndef NDEBUG
    ecs_map_fini(&index->component_name_map);
#endif
}

void ecs_entity_index_init(ecs_entity_index_t *index) {
    ecs_vec_init(&index->entities, sizeof(ecs_entity_record_t));
    index->first_available = UINT32_MAX;
}

void ecs_entity_index_fini(ecs_entity_index_t *index) {
    ecs_vec_fini(&index->entities);
}

#include <stdint.h>

#define ECS_BUILTIN_EVENT_COUNT 3 // OnAdd, OnRemove, OnSet

void ecs_observer_index_init(ecs_observer_index_t *index) {
    ecs_vec_init(&index->observers, sizeof(ecs_observer_t));
    index->event_count = ECS_BUILTIN_EVENT_COUNT;
}

void ecs_observer_index_fini(ecs_observer_index_t *index) {
    for (uint32_t i = 0; i < index->observers.size; i++) {
        ecs_observer_t *obs = ecs_vec_get_mut(&index->observers, i, ecs_observer_t);
        ecs_query_index_destroy(&obs->query);
    }
    ecs_vec_fini(&index->observers);
}

uint16_t ecs_observer_index_create(ecs_observer_index_t *index, const ecs_observer_desc_t *desc) {
    ecs_observer_t *obs = ecs_vec_push_empty(&index->observers, sizeof(ecs_observer_t));
    obs->event = desc->on;
    obs->callback = desc->callback;
    obs->user_data = desc->user_data;
    ecs_query_from_desc(&desc->query, &obs->query);
    return index->observers.size - 1;
}

void ecs_observer_index_match_tables(
    ecs_observer_index_t *index,
    ecs_table_t *tables,
    uint16_t table_count,
    uint16_t observer_id
) {
    ecs_observer_t *obs = ecs_vec_get_mut(&index->observers, observer_id, ecs_observer_t);
    for (uint16_t i = 0; i < table_count; i++) {
        if (ecs_query_match_table(&obs->query, &tables[i])) {
            ecs_table_add_observer(&tables[i], obs->event, observer_id);
        }
    }
}

void ecs_observer_index_add_table(ecs_observer_index_t *index, ecs_table_t *table) {
    for (uint32_t i = 0; i < index->observers.size; i++) {
        ecs_observer_t *obs = ecs_vec_get_mut(&index->observers, i, ecs_observer_t);
        if (ecs_query_match_table(&obs->query, table)) {
            ecs_table_add_observer(table, obs->event, i);
        }
    }
}

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void ecs_query_index_init(ecs_query_index_t *index) {
    ecs_vec_init(&index->queries, sizeof(ecs_query_cache_t));
}

void ecs_query_index_fini(ecs_query_index_t *index) {
    for (uint32_t i = 0; i < index->queries.size; i++) {
        ecs_query_cache_t *cache = ecs_vec_get_mut(&index->queries, i, ecs_query_cache_t);
        ecs_vec_fini(&cache->table_ids);
        ecs_vec_fini(&cache->fields);
        ecs_query_index_destroy(&cache->query);
    }
    ecs_vec_fini(&index->queries);
}

void ecs_query_index_destroy(ecs_query_t *query) {
    free(query->read);
    free(query->required);
    free(query->excluded);
}

static uint16_t ecs_query_count_terms(const ecs_component_t *ids) {
    uint16_t i = 0;
    while (ids[i]) {
        i++;
    }
    return i;
}

static ecs_component_t *ecs_query_copy_terms(const ecs_component_t *ids, uint16_t count) {
    if (count == 0) {
        return NULL;
    }
    ecs_component_t *copy = malloc(sizeof(ecs_component_t) * count);
    memcpy(copy, ids, sizeof(ecs_component_t) * count);
    return copy;
}

void ecs_query_from_desc(const ecs_query_desc_t *desc, ecs_query_t *query) {
    query->read_count = ecs_query_count_terms(desc->read);
    query->required_count = ecs_query_count_terms(desc->required);
    query->excluded_count = ecs_query_count_terms(desc->excluded);

    query->read = ecs_query_copy_terms(desc->read, query->read_count);
    query->required = ecs_query_copy_terms(desc->required, query->required_count);
    query->excluded = ecs_query_copy_terms(desc->excluded, query->excluded_count);

    query->bloom = 0;
    for (uint16_t i = 0; i < query->read_count; i++) {
        query->bloom |= 1ull << (query->read[i] % 64);
    }
    for (uint16_t i = 0; i < query->required_count; i++) {
        query->bloom |= 1ull << (query->required[i] % 64);
    }
}

static void
ecs_query_cache_add_table(ecs_query_cache_t *cache, const ecs_table_t *table, uint16_t table_id) {
    ecs_vec_push_u16(&cache->table_ids, table_id);
    for (uint16_t i = 0; i < cache->query.read_count; i++) {
        uint16_t col = ecs_table_get_column_index(table, cache->query.read[i]);
        void **slot = &table->cls[col].data;
        ecs_vec_push(&cache->fields, &slot, sizeof(void **));
    }
}

ecs_query_id_t ecs_query_index_create(ecs_query_index_t *index, const ecs_query_desc_t *desc) {
    ecs_assert(desc->read[0] != 0, "query must read at least one component\n");
    ecs_query_cache_t *query_cache = ecs_vec_push_empty(&index->queries, sizeof(ecs_query_cache_t));
    ecs_query_from_desc(desc, &query_cache->query);
    ecs_vec_init(&query_cache->table_ids, sizeof(uint16_t));
    ecs_vec_init(&query_cache->fields, sizeof(void **));

    return index->queries.size - 1;
}

void ecs_query_index_update_matches(ecs_world_t *world, ecs_query_cache_t *query_cache) {
    uint16_t component = query_cache->query.required_count != 0 ? query_cache->query.required[0]
                         : query_cache->query.read_count != 0   ? query_cache->query.read[0]
                                                                : 0;

    if (ECS_LIKELY(component)) {
        const ecs_vec_t *tables_vec =
            &ecs_component_index_get(&world->component_index, component)->tables;

        ecs_vec_iter(tables_vec, uint16_t, table_index, {
            const ecs_table_t *table = &world->table_index.tables[*table_index];

            if (ecs_query_match_table(&query_cache->query, table)) {
                ecs_query_cache_add_table(query_cache, table, *table_index);
            }
        });
    } else {
        const uint16_t table_count = world->table_index.table_count;
        const ecs_table_t *tables = world->table_index.tables;

        for (uint16_t i = 0; i < table_count; i++) {
            if (ecs_query_match_table(&query_cache->query, &tables[i])) {
                ecs_query_cache_add_table(query_cache, &tables[i], i);
            }
        }
    }
}

void ecs_query_index_add_table(
    ecs_query_index_t *index,
    const ecs_table_t *table,
    uint16_t table_id
) {
    for (uint32_t i = 0; i < index->queries.size; i++) {
        ecs_query_cache_t *cache = ecs_vec_get_mut(&index->queries, i, ecs_query_cache_t);
        if (ecs_query_match_table(&cache->query, table)) {
            ecs_query_cache_add_table(cache, table, table_id);
        }
    }
}

#include <stdint.h>
#include <stdlib.h>

static bool ecs_system_id_valid(const ecs_system_index_t *index, ecs_system_id_t system) {
    return system != 0 && system < index->systems.size;
}

static void ecs_system_index_plan_one(
    ecs_system_index_t *index,
    ecs_system_id_t system,
    uint8_t *state,
    ecs_vec_t *order
) {
    if (!ecs_system_id_valid(index, system)) {
        ecs_assert(false, "invalid system dependency: %u\n", system);
        return;
    }

    if (state[system] == 2) {
        return;
    }

    if (state[system] == 1) {
        ecs_assert(false, "system dependency cycle detected at system %u\n", system);
        return;
    }

    state[system] = 1;

    ecs_system_t *sys = ecs_system_index_get(index, system);
    for (uint32_t i = 0; i < 4; i++) {
        ecs_system_id_t after = sys->after[i];
        if (after == 0) {
            continue;
        }

        if (!ecs_system_id_valid(index, after)) {
            ecs_assert(false, "invalid system dependency: %u\n", after);
            continue;
        }

        ecs_system_t *dep = ecs_system_index_get(index, after);
        if (dep->phase != sys->phase) {
            ecs_assert(false, "system dependency must be in the same phase\n");
            continue;
        }

        ecs_system_index_plan_one(index, after, state, order);
    }

    state[system] = 2;

    if (sys->enabled) {
        ecs_vec_push_u16(order, system);
    }
}

void ecs_system_index_init(ecs_system_index_t *index) {
    ecs_vec_init(&index->systems, sizeof(ecs_system_t));
    ecs_vec_ensure(&index->systems, 1, sizeof(ecs_system_t));

    for (uint32_t i = 0; i < EcsPhaseCount; i++) {
        ecs_vec_init(&index->phase_order[i], sizeof(ecs_system_id_t));
    }

    index->plan_dirty = true;
}

ecs_system_id_t ecs_system_index_create(ecs_system_index_t *index, const ecs_system_t *system) {
    ecs_vec_push(&index->systems, system, sizeof(ecs_system_t));
    index->plan_dirty = true;
    return index->systems.size - 1;
}

ecs_system_t *ecs_system_index_get(ecs_system_index_t *index, ecs_system_id_t system) {
    ecs_assert(ecs_system_id_valid(index, system), "invalid system id: %u\n", system);
    return ecs_vec_get_mut(&index->systems, system, ecs_system_t);
}

void ecs_system_index_build_plan(ecs_system_index_t *index) {
    for (uint32_t i = 0; i < EcsPhaseCount; i++) {
        ecs_vec_clear(&index->phase_order[i]);
    }

    uint8_t *state = calloc(index->systems.size, sizeof(uint8_t));
    ecs_assert_not_null(state);

    for (uint32_t system = 1; system < index->systems.size; system++) {
        ecs_system_t *sys = ecs_system_index_get(index, system);
        ecs_assert(sys->phase < EcsPhaseCount, "invalid system phase: %u\n", sys->phase);

        if (sys->phase >= EcsPhaseCount) {
            continue;
        }

        ecs_system_index_plan_one(index, system, state, &index->phase_order[sys->phase]);
    }

    free(state);
    index->plan_dirty = false;
}

void ecs_system_index_fini(ecs_system_index_t *index) {
    for (uint32_t i = 0; i < EcsPhaseCount; i++) {
        ecs_vec_fini(&index->phase_order[i]);
    }

    ecs_vec_fini(&index->systems);
}

#include <stdint.h>
#include <stdlib.h>

#define INITIAL_SLOT_SHIFT 4
#define LOAD_FACTOR 0.75
#define ECS_TABLE_SLOT_EMPTY UINT16_MAX

static inline uint32_t ecs_type_hash(ecs_type_t type) {
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < type.count; ++i) {
        h ^= (uint32_t)type.ids[i];
        h *= 16777619u;
    }

    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

static inline uint16_t ecs_type_hash_fingerprint(uint32_t hash) {
    return (uint16_t)(hash ^ (hash >> 16));
}

static inline uint32_t ecs_table_index_slot_count(const ecs_table_index_t *map) {
    return 1u << map->slot_shift;
}

static inline void ecs_table_index_init_slots(ecs_table_index_t *map) {
    uint32_t slot_count = ecs_table_index_slot_count(map);
    map->slots = malloc(sizeof(ecs_type_slot_t) * slot_count);
    for (uint32_t i = 0; i < slot_count; ++i) {
        map->slots[i].table_index = ECS_TABLE_SLOT_EMPTY;
    }
}

static inline void
ecs_table_index_insert_slot(ecs_table_index_t *map, uint32_t hash, uint16_t table_index) {
    uint32_t slot_mask = ecs_table_index_slot_count(map) - 1;
    uint32_t slot_idx = hash & slot_mask;
    while (map->slots[slot_idx].table_index != ECS_TABLE_SLOT_EMPTY) {
        slot_idx = (slot_idx + 1) & slot_mask;
    }
    map->slots[slot_idx].hash = ecs_type_hash_fingerprint(hash);
    map->slots[slot_idx].table_index = table_index;
}

void ecs_table_index_init(ecs_table_index_t *map) {
    map->table_count = 0;
    map->table_capacity = 1;
    map->tables = malloc(sizeof(ecs_table_t) * map->table_capacity);
    map->slot_shift = INITIAL_SLOT_SHIFT;
    ecs_table_index_init_slots(map);
}

void ecs_table_index_fini(ecs_table_index_t *map) {
    for (uint16_t i = 0; i < map->table_count; i++) {
        ecs_table_fini(&map->tables[i]);
    }
    free(map->tables);
    free(map->slots);
}

static void ecs_table_index_resize(ecs_table_index_t *map) {
    ecs_type_slot_t *old_slots = map->slots;

    map->slot_shift += 1;
    ecs_table_index_init_slots(map);
    for (uint16_t i = 0; i < map->table_count; ++i) {
        ecs_table_index_insert_slot(map, ecs_type_hash(map->tables[i].type), i);
    }
    free(old_slots);
}

static void ecs_table_index_grow_tables(ecs_table_index_t *map) {
    map->table_capacity *= 2;
    map->tables = realloc(map->tables, sizeof(ecs_table_t) * map->table_capacity);
}

uint16_t ecs_table_index_get_or_create(ecs_world_t *world, ecs_type_t type) {
    const ecs_component_index_t *component_index = &world->component_index;
    ecs_table_index_t *map = &world->table_index;
    uint32_t hash = ecs_type_hash(type);
    uint16_t hash_fingerprint = ecs_type_hash_fingerprint(hash);
    uint32_t slot_mask = ecs_table_index_slot_count(map) - 1;
    uint32_t slot_idx = hash & slot_mask;

    // Fast path: lookup
    while (map->slots[slot_idx].table_index != ECS_TABLE_SLOT_EMPTY) {
        if (ECS_LIKELY(map->slots[slot_idx].hash == hash_fingerprint)) {
            const ecs_table_t *table = ecs_table_index_at(map, map->slots[slot_idx].table_index);
            if (ECS_LIKELY(
                    ecs_type_equals(table->type.ids, table->type.count, type.ids, type.count)
                )) {
                ecs_type_fini(&type);
                return (uint16_t)map->slots[slot_idx].table_index;
            }
        }
        slot_idx = (slot_idx + 1) & slot_mask;
    }

    // Slow path: creation
    if (ECS_UNLIKELY(map->table_count >= ecs_table_index_slot_count(map) * LOAD_FACTOR)) {
        ecs_table_index_resize(map);
        // re-calculate slot_idx after resize
        slot_mask = ecs_table_index_slot_count(map) - 1;
        slot_idx = hash & slot_mask;
    }
    if (ECS_UNLIKELY(map->table_count >= map->table_capacity)) {
        ecs_table_index_grow_tables(map);
    }

    uint16_t table_idx = map->table_count++;
    ecs_table_t new_table;
    ecs_table_init(&new_table, type, component_index, table_idx);
    map->tables[table_idx] = new_table;

    map->slots[slot_idx].hash = hash_fingerprint;
    map->slots[slot_idx].table_index = table_idx;

    ecs_query_index_add_table(&world->query_index, ecs_table_index_at(map, table_idx), table_idx);
    ecs_observer_index_add_table(&world->observer_index, ecs_table_index_at(map, table_idx));
    return (uint16_t)table_idx;
}

