#include "siecs.h"
#ifndef ECS_ADDONS_H
#define ECS_ADDONS_H

void init_rest(ecs_world_t *world);

#endif

#ifndef SIECS_DATASTRUCTURE_VEC_H
#define SIECS_DATASTRUCTURE_VEC_H
#ifndef SIECS_COMPILER_H
#define SIECS_COMPILER_H
#define ECS_LIKELY(x) __builtin_expect(!!(x), 1)
#define ECS_UNLIKELY(x) __builtin_expect(!!(x), 0)

#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    void *data;
    uint32_t size;
    uint32_t capacity;
} ecs_vec_t;

void ecs_vec_init(ecs_vec_t *vec, const uint32_t element_size);
void ecs_vec_init_w_size(ecs_vec_t *vec, const uint32_t element_size, uint32_t size);
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

#ifndef SIREFLECT_H
#include "sireflect.h"
#endif
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

#endif

#ifndef SIECS_ID_H
#define SIECS_ID_H
#include <stdint.h>

#define ecs_entity(index, generation) (((uint64_t)(index) << 32) | (generation & 0xffffffff))

#define ecs_first(id) ((uint32_t)((id) >> 32))
#define ecs_second(id) ((uint32_t)((id) & 0xffffffff))

#endif

#ifndef SIECS_TYPE_H
#define SIECS_TYPE_H
#include <stdint.h>
#include <string.h>

typedef struct {
    uint16_t *ids;
    uint16_t count;
    ecs_entity_t base;
} ecs_type_t;

ecs_type_t ecs_type_with_add(const ecs_type_t *type, uint16_t id);
ecs_type_t ecs_type_with_remove(const ecs_type_t *type, uint16_t id);
ecs_type_t ecs_type_with_remove_at(const ecs_type_t *type, uint16_t index);
ecs_type_t ecs_type_with_base(const ecs_type_t *type, ecs_entity_t base);

uint64_t ecs_type_bloom(const ecs_type_t *type);

// returns the index of the id in the type, or -1 if not found
int ecs_type_find(const ecs_type_t *type, uint16_t id);

void ecs_type_fini(ecs_type_t *type);

static inline int ecs_type_equals(const ecs_type_t *a, const ecs_type_t *b) {
    if (a->base != b->base)
        return 0;
    if (a->count != b->count)
        return 0;
    if (a->count == 0)
        return 1;
    return memcmp(a->ids, b->ids, (size_t)a->count * sizeof(uint16_t)) == 0;
}

#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    void *data;
    uint32_t size;
    uint16_t remove_edge; // the table that has the component removed or UINT16_MAX if the edge is
                          // not set
} ecs_column_t;

typedef struct ecs_table_s {
    ecs_id_map_t add_edge; // maps component id to the table that has the component added or column
                           // index if the component is in the table
    uint32_t entity_capacity;
    uint32_t entity_count;
    uint16_t data_count;
    ecs_entity_t *entities;
    ecs_column_t *cls;
    uint16_t *data_columns;
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

static inline void *
ecs_table_component_at_column(const ecs_table_t *table, uint16_t column_index, uint32_t row) {
    ecs_column_t *column = &table->cls[column_index];
    return column->size != 0 ? (uint8_t *)column->data + (column->size * row) : NULL;
}

static inline uint16_t
ecs_table_column_or_invalid(const ecs_table_t *table, ecs_component_t component_id) {
    uint16_t column_index = ecs_table_get_add_edge(table, component_id);
    if (column_index < table->type.count && table->type.ids[column_index] == component_id) {
        return column_index;
    }
    return UINT16_MAX;
}

bool ecs_table_has(
    const ecs_world_t *world,
    const ecs_table_t *table,
    ecs_component_t component_id
);
bool ecs_table_is_a(const ecs_world_t *world, const ecs_table_t *table, ecs_entity_t base);

static inline void copy_data_column(
    const ecs_column_t *restrict from,
    uint32_t from_row,
    ecs_column_t *restrict to,
    uint32_t to_row
) {
    memcpy(
        (uint8_t *)to->data + (from->size * to_row),
        (uint8_t *)from->data + (from->size * from_row),
        from->size
    );
}

static inline uint16_t
ecs_table_get_column_index(const ecs_table_t *table, ecs_component_t component_id) {
    return ecs_id_map_at(&table->add_edge, component_id);
}

static inline bool ecs_table_has_owned(const ecs_table_t *table, ecs_component_t component_id) {
    return ecs_table_column_or_invalid(table, component_id) != UINT16_MAX;
}

void *ecs_table_field(
    ecs_world_t *world,
    const ecs_table_t *table,
    ecs_component_t component_id,
    bool *is_shared
);

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

#ifndef SIECS_WORLD_INTERNAL_H
#define SIECS_WORLD_INTERNAL_H
#ifndef SIHTTP_H
#include "sihttp.h"
#endif
#ifndef SIREFLECT_H
#include "sireflect.h"
#endif
#ifndef SIECS_STORAGE_COMPONENT_INDEX_H
#define SIECS_STORAGE_COMPONENT_INDEX_H
#ifndef SIREFLECT_H
#include "sireflect.h"
#endif
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool registered;
    uint16_t *required;
    uint32_t required_count;
    uint32_t size;
    ecs_component_on_set_t on_set;
    ecs_component_on_remove_t on_remove;
    ecs_component_on_add_t on_add;
    uint32_t relation_flags;
    ecs_vec_t tables; // uint16_t
    sireflect_handle_t reflection;
    const sireflect_struct_desc_t *reflection_desc;
} ecs_component_record_t;

typedef struct ecs_component_index_s {
    ecs_vec_t components; // ecs_component_record_t
} ecs_component_index_t;

void ecs_component_index_register(
    ecs_component_index_t *index,
    ecs_component_t id,
    uint64_t size,
    ecs_component_on_set_t on_set,
    ecs_component_on_remove_t on_remove,
    ecs_component_on_add_t on_add,
    uint32_t relation_flags,
    sireflect_handle_t reflection,
    const sireflect_struct_desc_t *reflection_desc
);

#define ecs_component_index_get(index, id)                                                         \
    ecs_vec_get(&(index)->components, id, ecs_component_record_t)
#define ecs_component_index_get_mut(index, id)                                                     \
    ecs_vec_get_mut(&(index)->components, id, ecs_component_record_t)

void ecs_component_index_init(ecs_component_index_t *index);
void ecs_component_index_fini(ecs_component_index_t *index);

#endif

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
    ecs_vec_t entities;       // ecs_entity_record_t
    uint32_t first_available; // UINT32_MAX when no dead entity can be reused
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
        *record = (ecs_entity_record_t){ .generation = 0, .table_row = row, .table_id = 0 };
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
    record->table_id = UINT16_MAX;
    index->first_available = entity_id;
}

void ecs_entity_index_init(ecs_entity_index_t *index);
void ecs_entity_index_fini(ecs_entity_index_t *index);

#endif

#ifndef SIECS_STORAGE_MODULE_INDEX_H
#define SIECS_STORAGE_MODULE_INDEX_H

typedef struct {
    ecs_module_id_t *id;
    const char *name;
    ecs_vec_t observers;  // ecs_observer_id_t
    ecs_vec_t systems;    // ecs_system_id_t
    ecs_vec_t components; // ecs_component_t
    bool enabled;
} ecs_module_t;

typedef struct {
    ecs_vec_t modules; // ecs_module_t
} ecs_module_index_t;

void ecs_module_index_init(ecs_module_index_t *index);
void ecs_module_index_fini(ecs_module_index_t *index);

ecs_module_id_t ecs_module_index_create(
    ecs_module_index_t *index,
    ecs_module_id_t *id,
    const char *name
);
ecs_module_t *ecs_module_index_get(ecs_module_index_t *index, ecs_module_id_t module);
const ecs_module_t *ecs_module_index_get_const(
    const ecs_module_index_t *index,
    ecs_module_id_t module
);
ecs_module_id_t ecs_module_index_find(const ecs_module_index_t *index, const ecs_module_id_t *id);

#endif

#ifndef SIECS_STORAGE_OBSERVER_INDEX_H
#define SIECS_STORAGE_OBSERVER_INDEX_H
#ifndef SIECS_STORAGE_QUERY_INDEX_H
#define SIECS_STORAGE_QUERY_INDEX_H
#include <stdint.h>

typedef struct {
    uint64_t bloom;
    ecs_entity_t is_a;
    ecs_query_term_t *terms;
    ecs_query_term_t *fields;
    uint16_t term_count;
    uint16_t field_count;
} ecs_query_t;

typedef struct ecs_query_cache_s {
    ecs_query_t query;
    ecs_vec_t table_ids; // uint16_t
    void **fields_ptr;
    ecs_field_kind_t *fields_kind;
    uint16_t field_table_capacity;
    uint32_t active_index;
    uint16_t next_free;
    bool alive;
} ecs_query_cache_t;

typedef struct {
    ecs_vec_t queries;
    ecs_vec_t active_ids; // ecs_query_id_t
    uint16_t first_free;
} ecs_query_index_t;

void ecs_query_index_init(ecs_query_index_t *index);
void ecs_query_index_fini(ecs_query_index_t *index);
uint16_t ecs_query_index_create(ecs_query_index_t *index, const ecs_query_desc_t *desc);
void ecs_query_index_update_matches(ecs_world_t *world, ecs_query_cache_t *query_cache);
void ecs_query_index_add_table(ecs_world_t *world, const ecs_table_t *table, uint16_t table_id);

// Reusable query helpers shared with the observer index.
void ecs_query_from_desc(const ecs_query_desc_t *desc, ecs_query_t *query);
void ecs_query_index_destroy(ecs_query_t *query);

static inline bool ecs_query_term_requires_owned(ecs_query_term_t term) {
    return term.access == EcsOut || term.access == EcsInOut || term.access == EcsInOutOptional;
}

static inline bool ecs_query_match_table(
    const ecs_world_t *world,
    const ecs_query_t *query,
    const ecs_table_t *table
) {
    if (ECS_LIKELY((query->bloom & table->bloom) != query->bloom)) {
        return false;
    }

    if (query->is_a && !ecs_table_is_a(world, table, query->is_a)) {
        return false;
    }

    for (uint16_t i = 0; i < query->term_count; i++) {
        ecs_query_term_t term = query->terms[i];
        if (term.access == EcsInOptional || term.access == EcsInOutOptional) {
            continue;
        } else if (term.access == EcsNot) {
            if (ecs_table_has(world, table, term.id)) {
                return false;
            }
        } else if (ecs_query_term_requires_owned(term)) {
            if (ecs_table_column_or_invalid(table, term.id) == UINT16_MAX) {
                return false;
            }
        } else if (!ecs_table_has(world, table, term.id)) {
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
    bool enabled;
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
    ecs_world_t *world,
    ecs_table_t *tables,
    uint16_t table_count,
    uint16_t observer_id
);

// Cache every existing observer that matches a freshly created table.
void ecs_observer_index_add_table(ecs_world_t *world, ecs_table_t *table);

#endif

#ifndef SIECS_STORAGE_RESOURCE_INDEX_H
#define SIECS_STORAGE_RESOURCE_INDEX_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    ecs_resource_desc_t *records;
    void **data;
    bool *present;
    uint64_t capacity;
    uint64_t count;
} ecs_resource_index_t;

void ecs_resource_index_init(ecs_resource_index_t *index);
void ecs_resource_index_fini(ecs_resource_index_t *index, ecs_world_t *world);

ecs_resource_t ecs_resource_index_register(
    ecs_resource_index_t *index,
    const ecs_resource_desc_t *desc
);
ecs_resource_t ecs_resource_index_find(const ecs_resource_index_t *index, const char *name);
bool ecs_resource_index_is_registered(const ecs_resource_index_t *index, ecs_resource_t id);

void ecs_resource_index_set(
    ecs_resource_index_t *index,
    ecs_world_t *world,
    ecs_resource_t id,
    const void *data
);
void *ecs_resource_index_get(ecs_resource_index_t *index, ecs_resource_t id);
const void *ecs_resource_index_get_const(const ecs_resource_index_t *index, ecs_resource_t id);
bool ecs_resource_index_has(const ecs_resource_index_t *index, ecs_resource_t id);
void ecs_resource_index_remove(ecs_resource_index_t *index, ecs_world_t *world, ecs_resource_t id);

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
    ecs_module_index_t module_index;
    ecs_resource_index_t resource_index;
    ecs_module_id_t active_module;
    sireflect_registry_t *sireflect_registry;
    sihttp_server_t *server;
    ecs_world_feat_desc_t features;
    bool did_start;
    bool exit;
} ecs_world_t;

struct sihttp_app_state_s {
    ecs_world_t *world;
};

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
        if (!obs->enabled) {
            continue;
        }
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

ECS_RELATION_DEFINE(ChildOf, EcsRelationCascadeDelete);
ECS_COMPONENT_DEFINE(Name);
ECS_COMPONENT_DEFINE(Disabled);
ECS_COMPONENT_DEFINE(Abstract);

void ecs_bootstrap(ecs_world_t *world) {
    // Reserve identifiers used to represent false return values.
    ecs_table_index_get_or_create(world, (ecs_type_t){ 0 });
    ecs_vec_push_u64(&world->entity_index.entities, 0);
    ecs_component(world, { .name = "Invalid" });

    // Register the ecs_entity_t struct reflection.
    sireflect_register_struct(
        world->sireflect_registry,
        &(sireflect_struct_desc_t){
            .name = "ecs_entity_t",
            .fields = "{ uint32_t id; uint32_t generation; }",
            .size = sizeof(ecs_entity_t),
            .align = _Alignof(ecs_entity_t),
        }
    );

    ECS_COMPONENT_REGISTER(world, ChildOf);
    ECS_COMPONENT_REGISTER(world, Name);
    ECS_COMPONENT_REGISTER(world, Disabled);
    ECS_COMPONENT_REGISTER(world, Abstract);

    init_rest(world);
}

#ifndef SIECS_MODULE_H
#define SIECS_MODULE_H

void ecs_module_record_component(ecs_world_t *world, ecs_component_t component);
void ecs_module_record_system(ecs_world_t *world, ecs_system_id_t system);
void ecs_module_record_observer(ecs_world_t *world, ecs_observer_id_t observer);

#endif

#ifndef SIREFLECT_H
#include "sireflect.h"
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct ecs_component_global_name_s {
    char *name;
    ecs_component_t id;
    uint16_t count;
    struct ecs_component_global_name_s *next;
} ecs_component_global_name_t;

static ecs_component_t ecs_next_component_id = 1;

static ecs_component_t ecs_component_alloc_ids(uint16_t count) {
    ecs_component_t id = ecs_next_component_id;
    ecs_next_component_id += count;
    ecs_assert(ecs_next_component_id > id, "component id overflow\n");
    return id;
}

void RelationOnSet(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t target_component,
    const void *new_value,
    void *current_value
) {
    const RelationTarget *target_data = new_value;
    ecs_component_t source_component = target_component + 1;

    const RelationTarget *old_target_data = current_value;

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
    void *ptr
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
    void *ptr
) {
    (void)_entity;

    RelationSource *source_data = (void *)ptr;

    const ecs_entity_t *entities = source_data->entities.data;
    const uint32_t count = source_data->entities.size;
    const ecs_component_record_t *crec =
        ecs_component_index_get(&world->component_index, component);
    const bool cascade_delete = crec->relation_flags & EcsRelationCascadeDelete;

    // Prevent recursive calls to RelationOnRemove when removing relation from child
    source_data->entities.size = UINT32_MAX;
    for (uint32_t i = 0; i < count; i++) {
        if (cascade_delete) {
            ecs_kill(world, entities[i]);
        } else {
            ecs_remove_cid(world, entities[i], component - 1);
        }
    }

    ecs_vec_fini(&source_data->entities);
}

ecs_component_t
ecs_component_register(ecs_world_t *world, ecs_component_t *id, const ecs_component_desc_t *desc) {
    ecs_assert_not_null(world);
    ecs_assert_not_null(id);
    ecs_assert_not_null(desc);

    sireflect_handle_t reflection = SIREFLECT_INVALID_HANDLE;

    if (desc->struct_desc) {
        reflection = sireflect_try_register_struct(world->sireflect_registry, desc->struct_desc);
    }

    if (desc->relation_flags & EcsRelationTarget) {
        if (*id == 0) {
            *id = ecs_component_alloc_ids(2);
        }

        ecs_component_t component = *id;
        ecs_component_index_register(
            &world->component_index,
            component,
            desc->size,
            RelationOnSet,
            RelationOnRemove,
            desc->on_add,
            desc->relation_flags,
            reflection,
            desc->struct_desc
        );

        ecs_component_t source = component + 1;
        ecs_component_index_register(
            &world->component_index,
            source,
            desc->relation_flags & EcsRelationOneToOne ? sizeof(RelationTarget)
                                                       : sizeof(RelationSource),
            NULL,
            RelationSourceOnRemove,
            desc->on_add,
            (desc->relation_flags & ~EcsRelationTarget) | EcsRelationSource,
            SIREFLECT_INVALID_HANDLE,
            NULL
        );
        ecs_module_record_component(world, component);
        ecs_module_record_component(world, source);
        return component;
    } else {
        if (*id == 0) {
            *id = ecs_component_alloc_ids(1);
        }

        ecs_component_t component = *id;
        ecs_component_index_register(
            &world->component_index,
            component,
            desc->size,
            desc->on_set,
            desc->on_remove,
            desc->on_add,
            0,
            reflection,
            desc->struct_desc
        );
        ecs_module_record_component(world, component);
        return component;
    }
}

ecs_component_t ecs_component_init(ecs_world_t *world, const ecs_component_desc_t *desc) {
    ecs_component_t id = 0;
    return ecs_component_register(world, &id, desc);
}

ecs_entity_t ecs_new(ecs_world_t *world) {
    ecs_assert_not_null(world);
    ecs_table_t *table = ecs_get_table(world, 0);

    ecs_entity_t entity = ecs_entity_index_create(&world->entity_index, table->entity_count);
    ecs_table_add_entity(table, entity);

    return entity;
}

bool ecs_is_alive(const ecs_world_t *world, ecs_entity_t entity) {
    return ecs_entity_index_is_alive(&world->entity_index, entity);
}

#ifndef NDEBUG
static inline bool
ecs_would_create_base_cycle(const ecs_world_t *world, ecs_entity_t entity, ecs_entity_t target) {
    while (target != 0) {
        if (target == entity) {
            return true;
        }
        const ecs_entity_record_t *target_record = ecs_get_record(world, target);
        const ecs_table_t *target_table = ecs_get_table(world, target_record->table_id);
        target = target_table->type.base;
    }
    return false;
}
#endif

static inline void ecs_entity_rebase(
    ecs_world_t *world,
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_table_id
) {
    ecs_table_t *to_table = ecs_get_table(world, to_table_id);
    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    for (uint16_t i = 0; i < from_table->data_count; i++) {
        uint16_t col = from_table->data_columns[i];
        copy_data_column(&from_table->cls[col], old_row, &to_table->cls[col], new_row);
    }

    ecs_entity_t moved = ecs_table_remove_entity(from_table, old_row);
    if (moved != entity) {
        ecs_get_record(world, moved)->table_row = old_row;
    }

    record->table_id = to_table_id;
    record->table_row = new_row;
}

bool ecs_is(ecs_world_t *world, ecs_entity_t entity, ecs_entity_t target) {
    return ecs_get_table(world, ecs_get_record(world, entity)->table_id)->type.base == target;
}

void ecs_is_a(ecs_world_t *world, ecs_entity_t entity, ecs_entity_t target) {
    ecs_assert_not_null(world);
    ecs_assert_entity_valid(entity);
    ecs_assert_entity_valid(target);
    ecs_assert_is_alive(world, entity);
    ecs_assert_is_alive(world, target);
    ecs_assert(entity != target, "entity cannot inherit itself: %d\n", ecs_first(entity));
    ecs_assert(
        !ecs_would_create_base_cycle(world, entity, target),
        "cyclic inheritance: %d inherits from %d\n",
        ecs_first(entity),
        ecs_first(target)
    );
    ecs_assert(
        ecs_has_cid_owned(world, target, ecs_id(Abstract)),
        "An entity can only inherit from an abstract entity."
    );

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    uint16_t from_table_id = record->table_id;
    ecs_table_t *from_table = ecs_get_table(world, from_table_id);
    if (from_table->type.base == target) {
        return;
    }

    ecs_type_t new_type = ecs_type_with_base(&from_table->type, target);
    uint16_t to_table_id = ecs_table_index_get_or_create(world, new_type);
    if (to_table_id == from_table_id) {
        return;
    }

    from_table = ecs_get_table(world, from_table_id);
    ecs_entity_rebase(world, record, entity, from_table, to_table_id);
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
                ecs_table_component_at_column(table, (uint16_t)i, record->table_row)
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

ecs_module_id_t ecs_module_init(ecs_world_t *world, const ecs_module_desc_t *desc) {
    ecs_assert_not_null(world);
    ecs_assert_not_null(desc);
    ecs_assert_not_null(desc->name);
    ecs_assert_not_null(desc->import);

    ecs_module_id_t existing = ecs_module_index_find(&world->module_index, desc->id);
    if (existing) {
        return existing;
    }

    ecs_module_id_t module = ecs_module_index_create(&world->module_index, desc->id, desc->name);
    if (desc->id) {
        *desc->id = module;
    }

    ecs_module_id_t prev = world->active_module;
    world->active_module = module;
    desc->import(world, desc->desc);
    world->active_module = prev;

    if (desc->disabled) {
        ecs_module_disable(world, module);
    }

    return module;
}

ecs_module_id_t ecs_module_find(ecs_world_t *world, const ecs_module_id_t *id) {
    ecs_assert_not_null(world);
    return ecs_module_index_find(&world->module_index, id);
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

ecs_event_t ecs_event(ecs_world_t *world) { return world->observer_index.event_count++; }

ecs_observer_id_t ecs_observer_init(ecs_world_t *world, const ecs_observer_desc_t *desc) {
    ecs_assert_not_null(world);
    ecs_assert(desc->callback != NULL, "Observer callback cannot be NULL");
    ecs_observer_id_t oid = ecs_observer_index_create(&world->observer_index, desc);
    ecs_observer_index_match_tables(
        world,
        world->table_index.tables,
        world->table_index.table_count,
        oid
    );
    ecs_module_record_observer(world, oid);
    return oid;
}

void ecs_observer_enable(ecs_world_t *world, ecs_observer_id_t id) {
    ecs_vec_get_mut(&world->observer_index.observers, id, ecs_observer_t)->enabled = true;
}

void ecs_observer_disable(ecs_world_t *world, ecs_observer_id_t id) {
    ecs_vec_get_mut(&world->observer_index.observers, id, ecs_observer_t)->enabled = false;
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

static void ecs_query_index_remove_active_id(ecs_query_index_t *index, ecs_query_id_t qid) {
    ecs_query_cache_t *cache = ecs_vec_get_mut(&index->queries, qid, ecs_query_cache_t);
    uint32_t active_index = cache->active_index;
    uint32_t last_index = index->active_ids.size - 1;

    if (active_index != last_index) {
        ecs_query_id_t moved = *ecs_vec_get(&index->active_ids, last_index, ecs_query_id_t);
        ((ecs_query_id_t *)index->active_ids.data)[active_index] = moved;
        ecs_vec_get_mut(&index->queries, moved, ecs_query_cache_t)->active_index = active_index;
    }

    ecs_vec_remove_last(&index->active_ids);
}

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
    ecs_assert(query_id < world->query_index.queries.size, "invalid query id: %u\n", query_id);

    ecs_query_cache_t *cache =
        ecs_vec_get_mut(&world->query_index.queries, query_id, ecs_query_cache_t);
    ecs_assert(cache->alive, "query id is not alive: %u\n", query_id);
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
    if (it->cache->query.field_count == 0) {
        it->ptrs = NULL;
        it->field_kinds = NULL;
    } else {
        it->ptrs = &it->cache->fields_ptr[it->table_idx * it->cache->query.field_count];
        it->field_kinds = &it->cache->fields_kind[it->table_idx * it->cache->query.field_count];
    }
    it->entities = it->world->table_index.tables[tids[it->table_idx]].entities;
    return true;
}

ecs_table_t *ecs_iter_table(ecs_iter_t *it) {
    uint16_t tid = *ecs_vec_get_mut(&it->cache->table_ids, it->table_idx, uint16_t);
    return ecs_table_index_at(&it->world->table_index, tid);
}

void ecs_query_fini(ecs_world_t *world, ecs_query_id_t qid) {
    ecs_assert_not_null(world);
    ecs_assert(qid < world->query_index.queries.size, "invalid query id: %u\n", qid);

    ecs_query_cache_t *cache = ecs_vec_get_mut(&world->query_index.queries, qid, ecs_query_cache_t);
    ecs_assert(cache->alive, "query id is not alive: %u\n", qid);

    ecs_query_index_destroy(&cache->query);
    free(cache->fields_ptr);
    free(cache->fields_kind);
    ecs_vec_fini(&cache->table_ids);
    cache->fields_ptr = NULL;
    cache->fields_kind = NULL;
    cache->field_table_capacity = 0;

    ecs_query_index_remove_active_id(&world->query_index, qid);
    cache->next_free = world->query_index.first_free;
    cache->alive = false;
    world->query_index.first_free = qid;
}

ecs_resource_t ecs_resource_init(ecs_world_t *world, const ecs_resource_desc_t *desc) {
    ecs_assert_not_null(world);
    return ecs_resource_index_register(&world->resource_index, desc);
}

ecs_resource_t ecs_resource_find(ecs_world_t *world, const char *name) {
    ecs_assert_not_null(world);
    return ecs_resource_index_find(&world->resource_index, name);
}

bool ecs_resource_is_registered_rid(const ecs_world_t *world, ecs_resource_t id) {
    ecs_assert_not_null(world);
    return ecs_resource_index_is_registered(&world->resource_index, id);
}

void ecs_set_resource_rid(ecs_world_t *world, ecs_resource_t id, const void *data) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(id);
    ecs_assert_not_null(data);

    ecs_resource_index_set(&world->resource_index, world, id, data);
}

void *ecs_resource_rid(ecs_world_t *world, ecs_resource_t id) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(id);

    void *resource = ecs_resource_index_get(&world->resource_index, id);
    ecs_assert(resource != NULL, "resource does not exist: %d\n", id);
    return resource;
}

void *ecs_try_resource_rid(ecs_world_t *world, ecs_resource_t id) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(id);

    return ecs_resource_index_get(&world->resource_index, id);
}

bool ecs_has_resource_rid(const ecs_world_t *world, ecs_resource_t id) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(id);

    return ecs_resource_index_has(&world->resource_index, id);
}

void ecs_remove_resource_rid(ecs_world_t *world, ecs_resource_t id) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(id);

    ecs_resource_index_remove(&world->resource_index, world, id);
}

#ifndef SIHTTP_H
#include "sihttp.h"
#endif
#include <stdio.h>
#include <time.h>

#define ECS_SYSTEM_NO_QUERY UINT16_MAX

ecs_system_id_t ecs_system_init(ecs_world_t *world, const ecs_system_desc_t *desc) {
    ecs_assert_not_null(world);
    ecs_assert_not_null(desc);
    ecs_assert(desc->callback, "system requires callback function\n");
    ecs_assert(desc->phase < EcsPhaseCount, "invalid system phase: %u\n", desc->phase);

    ecs_system_t sys = {
        .name = desc->name,
        .qid = desc->query.terms[0].id ? ecs_query_init(world, &desc->query) : ECS_SYSTEM_NO_QUERY,
        .callback = desc->callback,
        .phase = desc->phase,
        .enabled = !desc->disabled,
    };

    memcpy(sys.after, desc->after, sizeof(ecs_system_id_t[4]));

    ecs_system_id_t system = ecs_system_index_create(&world->system_index, &sys);
    ecs_module_record_system(world, system);
    return system;
}

void ecs_run_system(ecs_world_t *world, ecs_system_id_t system) {
    ecs_assert_not_null(world);

    ecs_system_t *sys = ecs_system_index_get(&world->system_index, system);
    if (!sys->enabled) {
        return;
    }

    if (sys->qid != ECS_SYSTEM_NO_QUERY) {
        ecs_iter_t it = ecs_query_iter(world, sys->qid);
        while (ecs_iter_next(&it)) {
            sys->callback(&it);
        }
    } else {
        ecs_iter_t it = {
            .world = world,
            .count = 1,
        };
        sys->callback(&it);
    }
}

void ecs_run_phase(ecs_world_t *world, ecs_phase_t phase) {
    ecs_assert_not_null(world);
    ecs_assert(phase < EcsPhaseCount, "invalid system phase: %u\n", phase);

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
static inline double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static inline void sleep_sec(double seconds) {
    if (seconds <= 0.0)
        return;

    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1000000000.0);

    nanosleep(&ts, NULL);
}

bool ecs_progress(ecs_world_t *world) {
    ecs_assert_not_null(world);

    double frame_start = now_sec();

    if (!world->did_start) {
        ecs_run_phase(world, EcsPreStart);
        ecs_run_phase(world, EcsStart);
        ecs_run_phase(world, EcsPostStart);
        world->did_start = true;
    }

    for (ecs_phase_t phase = EcsOnLoad; phase < EcsPhaseCount; phase++) {
        ecs_run_phase(world, phase);
    }
    if (world->features.rest) {
        sihttp_server_poll(world->server);
    }

    if (world->features.target_fps) {
        double target_dt = 1.0 / (double)world->features.target_fps;
        double elapsed = now_sec() - frame_start;
        double remaining = target_dt - elapsed;

        sleep_sec(remaining);
    }

    return !world->exit;
}

void ecs_system_enable(ecs_world_t *world, ecs_system_id_t system) {
    ecs_assert_not_null(world);

    ecs_system_t *sys = ecs_system_index_get(&world->system_index, system);
    if (sys->enabled == true) {
        return;
    }

    sys->enabled = true;
    world->system_index.plan_dirty = true;
}

void ecs_system_disable(ecs_world_t *world, ecs_system_id_t system) {
    ecs_assert_not_null(world);

    ecs_system_t *sys = ecs_system_index_get(&world->system_index, system);
    if (sys->enabled == false) {
        return;
    }

    sys->enabled = false;
    world->system_index.plan_dirty = true;
}

void ecs_table_init(
    ecs_table_t *table,
    ecs_type_t type,
    const ecs_component_index_t *component_index,
    uint16_t table_id
) {
    table->type = type;
    table->entity_capacity = 1;
    table->entity_count = 0;
    table->data_count = 0;
    table->entities = malloc(sizeof(ecs_entity_t) * table->entity_capacity);
    table->cls = type.count == 0 ? NULL : malloc(sizeof(ecs_column_t) * type.count);
    table->data_columns = type.count == 0 ? NULL : malloc(sizeof(uint16_t) * type.count);
    table->bloom = ecs_type_bloom(&type);

    ecs_vec_init(&table->observers_by_event, sizeof(ecs_vec_t));
    ecs_id_map_init(&table->add_edge);

    for (uint16_t i = 0; i < type.count; i++) {
        ecs_component_record_t *rec = ecs_component_index_get_mut(component_index, type.ids[i]);
        ecs_vec_push_u16(&rec->tables, table_id);
        table->cls[i].size = rec->size;
        table->cls[i].data = rec->size != 0 ? calloc(table->entity_capacity, rec->size) : NULL;
        if (rec->size != 0) {
            table->data_columns[table->data_count++] = i;
        }
        ecs_id_map_set(&table->add_edge, type.ids[i], i);
        table->cls[i].remove_edge = UINT16_MAX;
    }

    if (table->data_count == 0) {
        free(table->data_columns);
        table->data_columns = NULL;
    } else if (table->data_count < type.count) {
        table->data_columns = realloc(table->data_columns, sizeof(uint16_t) * table->data_count);
    }
}

static inline void ecs_table_grow(ecs_table_t *table) {
    uint64_t new_capacity = table->entity_capacity * (uint64_t)2;
    table->entities = realloc(table->entities, sizeof(ecs_entity_t) * new_capacity);
    for (uint16_t i = 0; i < table->data_count; i++) {
        ecs_column_t *column = &table->cls[table->data_columns[i]];
        column->data = realloc(column->data, column->size * new_capacity);
        memset(
            (uint8_t *)column->data + (column->size * table->entity_capacity),
            0,
            column->size * (new_capacity - table->entity_capacity)
        );
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
        for (uint16_t i = 0; i < table->data_count; i++) {
            ecs_column_t *column = &table->cls[table->data_columns[i]];
            const void *src = (char *)column->data + (column->size * last_row);
            void *dst = (char *)column->data + (column->size * row);
            memcpy(dst, src, column->size);
        }
        table->entity_count -= 1;
        return moved_entity;
    }
    table->entity_count -= 1;
    return removed_entity;
}

void *ecs_table_get_component(ecs_table_t *table, ecs_component_t component_id, uint32_t row) {
    return ecs_table_component_at_column(
        table,
        ecs_table_get_column_index(table, component_id),
        row
    );
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
    free(table->data_columns);
    ecs_type_fini(&table->type);
}

bool ecs_table_has(
    const ecs_world_t *world,
    const ecs_table_t *table,
    ecs_component_t component_id
) {
    if (ecs_table_column_or_invalid(table, component_id) != UINT16_MAX) {
        return true;
    }

    if (component_id == ecs_id(Abstract)) {
        return false;
    }

    ecs_entity_t base = table->type.base;
    while (base != 0) {
        const ecs_entity_record_t *record = ecs_get_record(world, base);
        const ecs_table_t *base_table = ecs_get_table(world, record->table_id);
        if (ecs_table_column_or_invalid(base_table, component_id) != UINT16_MAX) {
            return true;
        }
        base = base_table->type.base;
    }

    return false;
}

bool ecs_table_is_a(const ecs_world_t *world, const ecs_table_t *table, ecs_entity_t base) {
    if (base == 0) {
        return true;
    }

    ecs_entity_t current = table->type.base;
    while (current != 0) {
        if (current == base) {
            return true;
        }

        const ecs_entity_record_t *record = ecs_get_record(world, current);
        const ecs_table_t *base_table = ecs_get_table(world, record->table_id);
        current = base_table->type.base;
    }

    return false;
}

void *ecs_table_field(
    ecs_world_t *world,
    const ecs_table_t *table,
    ecs_component_t component_id,
    bool *is_shared
) {
    uint16_t cidx = ecs_table_column_or_invalid(table, component_id);
    if (cidx != UINT16_MAX) {
        *is_shared = false;
        return &table->cls[cidx].data;
    }

    ecs_entity_t base = table->type.base;
    while (base != 0) {
        const ecs_entity_record_t *record = ecs_get_record(world, base);
        const ecs_table_t *base_table = ecs_get_table(world, record->table_id);

        cidx = ecs_table_column_or_invalid(base_table, component_id);
        if (cidx != UINT16_MAX) {
            *is_shared = true;
            return ecs_table_component_at_column(base_table, cidx, record->table_row);
        }

        base = base_table->type.base;
    }

    *is_shared = false;
    return NULL;
}

ecs_type_t ecs_type_with_add(const ecs_type_t *type, uint16_t id) {
    ecs_type_t new_type = {
        .ids = malloc((type->count + 1) * sizeof(uint16_t)),
        .count = type->count + 1,
        .base = type->base,
    };

    uint16_t i = 0;
    while (i < type->count && type->ids[i] < id) {
        new_type.ids[i] = type->ids[i];
        i++;
    }
    new_type.ids[i] = id;
    if (i < type->count) {
        memcpy(&new_type.ids[i + 1], &type->ids[i], (type->count - i) * sizeof(uint16_t));
    }

    return new_type;
}

ecs_type_t ecs_type_with_remove(const ecs_type_t *type, uint16_t id) {
    for (uint16_t i = 0; i < type->count; i++) {
        if (type->ids[i] == id) {
            return ecs_type_with_remove_at(type, i);
        }
    }
    return (ecs_type_t){ .base = type->base };
}

ecs_type_t ecs_type_with_remove_at(const ecs_type_t *type, uint16_t index) {
    ecs_type_t new_type = {
        .ids = malloc((type->count - 1) * sizeof(uint16_t)),
        .count = type->count - 1,
        .base = type->base,
    };
    if (index > 0) {
        memcpy(new_type.ids, type->ids, index * sizeof(uint16_t));
    }
    if (index + 1 < type->count) {
        memcpy(
            &new_type.ids[index],
            &type->ids[index + 1],
            (type->count - index - 1) * sizeof(uint16_t)
        );
    }
    return new_type;
}

ecs_type_t ecs_type_with_base(const ecs_type_t *type, ecs_entity_t base) {
    ecs_type_t new_type = {
        .ids = type->count == 0 ? NULL : malloc(type->count * sizeof(uint16_t)),
        .count = type->count,
        .base = base,
    };
    if (type->count != 0) {
        memcpy(new_type.ids, type->ids, type->count * sizeof(uint16_t));
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

#ifndef SIHTTP_H
#include "sihttp.h"
#endif
#ifndef SIREFLECT_H
#include "sireflect.h"
#endif

#define ecs_assert_can_be_updated(world, entity, ...)                                              \
    ecs_assert(!ecs_has_cid_owned(world, entity, ecs_id(Abstract)), __VA_ARGS__)

ecs_world_t *ecs_init_w_features(const ecs_world_feat_desc_t *features) {
    ecs_world_t *world = malloc(sizeof(ecs_world_t));
    ecs_entity_index_init(&world->entity_index);
    ecs_component_index_init(&world->component_index);
    ecs_table_index_init(&world->table_index);
    ecs_query_index_init(&world->query_index);
    ecs_observer_index_init(&world->observer_index);
    ecs_system_index_init(&world->system_index);
    ecs_module_index_init(&world->module_index);
    ecs_resource_index_init(&world->resource_index);
    world->active_module = 0;
    world->features = *features;
    world->did_start = false;
    world->exit = false;
    world->server = NULL;

    world->sireflect_registry = sireflect_registry_init();

    ecs_bootstrap(world);
    return world;
}

ecs_world_t *ecs_init() {
    ecs_world_t *world = ecs_with_features({});

    return world;
}

static inline void copy_column(
    const ecs_column_t *restrict from,
    const uint32_t from_row,
    ecs_column_t *restrict to,
    const uint32_t to_row
) {
    if (from->size == 0)
        return;
    memcpy(
        (uint8_t *)to->data + (from->size * to_row),
        (uint8_t *)from->data + (from->size * from_row),
        from->size
    );
}

static inline void finish_migration(
    const ecs_world_t *world,
    ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    const uint32_t old_row,
    const uint16_t to_table_id,
    const uint32_t new_row
) {
    ecs_entity_t moved = ecs_table_remove_entity(from_table, old_row);
    if (moved != entity) {
        ecs_get_record(world, moved)->table_row = old_row;
    }

    record->table_id = to_table_id;
    record->table_row = new_row;
}

// Generic migration: move an entity from its current table to an arbitrary
// target table, without knowing which components were added or removed, or how
// many. Both type id arrays are sorted ascending, so a sorted merge classifies
// each column: shared -> copy, only-in-from -> removed (skip), only-in-to ->
// added (zero). Pure data movement; callers own events/hooks.
static inline void migrate_entity(
    ecs_world_t *world,
    ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    const uint16_t to_id
) {
    ecs_table_t *to_table = ecs_get_table(world, to_id);

    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    uint16_t fi = 0, ti = 0;
    while (fi < from_table->type.count && ti < to_table->type.count) {
        uint16_t fid = from_table->type.ids[fi];
        uint16_t tid = to_table->type.ids[ti];
        if (fid == tid) {
            copy_column(&from_table->cls[fi], old_row, &to_table->cls[ti], new_row);
            fi++;
            ti++;
        } else if (fid < tid) {
            fi++;
        } else {
            ecs_column_t *c = &to_table->cls[ti];
            if (c->size != 0)
                memset((uint8_t *)c->data + (c->size * new_row), 0, c->size);
            ti++;
        }
    }
    for (; ti < to_table->type.count; ti++) {
        ecs_column_t *c = &to_table->cls[ti];
        if (c->size != 0)
            memset((uint8_t *)c->data + (c->size * new_row), 0, c->size);
    }

    finish_migration(world, record, entity, from_table, old_row, to_id, new_row);
}

static inline void *migrate_entity_add(
    const ecs_world_t *world,
    ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    ecs_table_t *to_table,
    const uint16_t to_table_id,
    const ecs_component_t added_id
) {
    const uint32_t old_row = record->table_row;
    const uint32_t new_row = ecs_table_add_entity(to_table, entity);

    const uint16_t k = ecs_table_get_column_index(to_table, added_id);
    ecs_column_t *added = &to_table->cls[k];
    if (added->size != 0) {
        memset((uint8_t *)added->data + (added->size * new_row), 0, added->size);
    }

    uint16_t i = 0;
    for (; i < from_table->data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        if (from_col >= k) {
            break;
        }
        copy_data_column(&from_table->cls[from_col], old_row, &to_table->cls[from_col], new_row);
    }
    for (; i < from_table->data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        copy_data_column(
            &from_table->cls[from_col],
            old_row,
            &to_table->cls[from_col + 1],
            new_row
        );
    }

    finish_migration(world, record, entity, from_table, old_row, to_table_id, new_row);
    return ecs_table_component_at_column(to_table, k, new_row);
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

    uint16_t i = 0;
    for (; i < from_table->data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        if (from_col >= col_idx) {
            break;
        }
        copy_data_column(&from_table->cls[from_col], old_row, &to_table->cls[from_col], new_row);
    }
    if (i < from_table->data_count && from_table->data_columns[i] == col_idx) {
        i++;
    }
    for (; i < from_table->data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        copy_data_column(
            &from_table->cls[from_col],
            old_row,
            &to_table->cls[from_col - 1],
            new_row
        );
    }

    finish_migration(world, record, entity, from_table, old_row, to_id, new_row);
}

void ecs_add_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);
    ecs_assert_can_be_updated(world, entity, "An abstract entity cannot be updated.");

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    uint16_t from_id = record->table_id;
    ecs_table_t *table = ecs_get_table(world, from_id);

    uint16_t edge = ecs_table_get_add_edge(table, cid);

    if (ECS_UNLIKELY(edge < table->type.count && table->type.ids[edge] == cid)) {
        return; // cid already present
    }

    const ecs_component_record_t *crec = ecs_component_index_get(&world->component_index, cid);
    if (crec->required_count) {
        for (uint32_t i = 0; i < crec->required_count; i++) {
            ecs_add_cid(world, entity, crec->required[i]);
        }

        from_id = record->table_id;
        table = ecs_get_table(world, from_id);
        edge = ecs_table_get_add_edge(table, cid);
        if (ECS_UNLIKELY(edge < table->type.count && table->type.ids[edge] == cid)) {
            return;
        }
    }

    if (edge == UINT16_MAX) {
        const ecs_type_t new_type = ecs_type_with_add(&table->type, cid);
        edge = ecs_table_index_get_or_create(world, new_type);

        // Re-fetch: ecs_table_index_get_or_create may realloc the tables vec
        table = ecs_get_table(world, from_id);
        ecs_id_map_set(&table->add_edge, cid, edge);
    } else if (ECS_UNLIKELY(edge < table->type.count && table->type.ids[edge] == cid)) {
        return;
    }

    ecs_table_t *new_table = ecs_get_table(world, edge);
    void *component_data = migrate_entity_add(world, record, entity, table, new_table, edge, cid);

    if (crec->on_add) {
        crec->on_add(world, entity, cid, component_data);
        new_table = ecs_get_table(world, record->table_id);
    }
    ecs_emit(world, new_table, entity, EcsOnAdd, component_data);
}

void ecs_remove_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    uint16_t from_id = record->table_id;
    ecs_table_t *table = ecs_get_table(world, from_id);

    uint16_t col_idx = ecs_table_column_or_invalid(table, cid);

    if (ECS_UNLIKELY(col_idx == UINT16_MAX)) {
        return;
    }

    uint16_t new_table_id = table->cls[col_idx].remove_edge;
    if (new_table_id == UINT16_MAX) {
        ecs_type_t new_type = ecs_type_with_remove_at(&table->type, col_idx);
        new_table_id = ecs_table_index_get_or_create(world, new_type);
        // Re-fetch: ecs_table_index_get_or_create may realloc the tables vec
        table = ecs_get_table(world, from_id);
        table->cls[col_idx].remove_edge = new_table_id;
    }

    void *removed_data = ecs_table_component_at_column(table, col_idx, record->table_row);

    const ecs_component_record_t *crec = ecs_component_index_get(&world->component_index, cid);
    if (crec->on_remove) {
        crec->on_remove(world, entity, cid, removed_data);
        table = ecs_get_table(world, from_id);
    }
    ecs_emit(world, table, entity, EcsOnRemove, removed_data);

    migrate_entity_remove(world, record, entity, table, new_table_id, (uint16_t)col_idx);
}

void *ecs_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    const ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);
    return ecs_table_component_at_column(
        table,
        ecs_table_get_column_index(table, cid),
        record->table_row
    );
}

void *ecs_try_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    const ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);

    uint16_t col_idx = ecs_table_column_or_invalid(table, cid);
    if (col_idx != UINT16_MAX) {
        return ecs_table_component_at_column(table, col_idx, record->table_row);
    }
    return NULL;
}

void ecs_set_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid, const void *data) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_add_cid(world, entity, cid);
    const ecs_component_record_t *crec = ecs_component_index_get(&world->component_index, cid);
    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);
    uint16_t col_idx = ecs_table_get_column_index(table, cid);
    void *dst = ecs_table_component_at_column(table, col_idx, record->table_row);

    // on_set sees the new input data and the current table slot before copy.
    if (crec->on_set) {
        crec->on_set(world, entity, cid, data, dst);
        record = ecs_get_record(world, entity);
        table = ecs_get_table(world, record->table_id);
    }
    ecs_emit(world, table, entity, EcsOnSet, data);
    if (crec->size != 0) {
        memcpy(dst, data, crec->size);
    }
}

bool ecs_has_cid(const ecs_world_t *world, ecs_entity_t entity, ecs_component_t id) {
    ecs_assert_not_null(world);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    uint16_t tid = ecs_get_record(world, entity)->table_id;
    return ecs_table_has(world, ecs_get_table(world, tid), id);
}

bool ecs_has_cid_owned(const ecs_world_t *world, ecs_entity_t entity, ecs_component_t id) {
    ecs_assert_not_null(world);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    uint16_t tid = ecs_get_record(world, entity)->table_id;
    return ecs_table_has_owned(ecs_get_table(world, tid), id);
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
    ecs_resource_index_fini(&world->resource_index, world);
    ecs_entity_index_fini(&world->entity_index);
    ecs_component_index_fini(&world->component_index);
    ecs_table_index_fini(&world->table_index);
    ecs_query_index_fini(&world->query_index);
    ecs_observer_index_fini(&world->observer_index);
    ecs_system_index_fini(&world->system_index);
    ecs_module_index_fini(&world->module_index);
    sireflect_registry_fini(world->sireflect_registry);

    if (world->features.rest) {
        sihttp_server_stop(world->server);
    }
    sihttp_server_fini(world->server);

    free(world);
}

void ecs_clone_w_entity(ecs_world_t *world, ecs_entity_t entity, ecs_entity_t target) {
    const ecs_entity_record_t *target_record = ecs_get_record(world, target);
    ecs_table_t *target_table = ecs_get_table(world, target_record->table_id);

    ecs_entity_record_t *entity_record = ecs_get_record(world, entity);
    ecs_table_t *entity_table = ecs_get_table(world, entity_record->table_id);

    ecs_table_add_entity(target_table, entity);

    migrate_entity(world, entity_record, entity, entity_table, target_record->table_id);
}

#ifndef SIECS_ADDONS_REST_INTERNAL_H
#define SIECS_ADDONS_REST_INTERNAL_H

#ifndef SIHTTP_H
#include "sihttp.h"
#endif
#ifndef SIJSON_H
#include "sijson.h"
#endif
#ifndef SIREFLECT_H
#include "sireflect.h"
#endif

sihttp_response_t ecs_rest_json_response(int status, sijson_value_t body);
sihttp_response_t ecs_rest_error_response(int status, const char *message);

sijson_value_t ecs_rest_entity_json(ecs_world_t *world, ecs_entity_t entity);
sijson_value_t ecs_rest_entity_children_json(ecs_world_t *world, ecs_entity_t entity);
sijson_value_t ecs_rest_entity_detail_json(ecs_world_t *world, ecs_entity_t entity);
bool ecs_rest_entity_component_is_reflected(ecs_world_t *world, ecs_component_t component);
sijson_value_t
ecs_rest_entity_component_json(ecs_world_t *world, ecs_component_t component, const void *ptr);
sihttp_response_t ecs_rest_set_entity_component(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t component,
    const char *body
);

sihttp_response_t ecs_rest_get_entities(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_entity(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_entity_children(const sihttp_request_t *req);
sihttp_response_t ecs_rest_put_entity_component(const sihttp_request_t *req);
sihttp_response_t ecs_rest_get_schema(const sihttp_request_t *req);
sihttp_response_t ecs_rest_post_entities(const sihttp_request_t *req);

#endif

#ifndef SIHTTP_H
#include "sihttp.h"
#endif

sihttp_response_t health(const sihttp_request_t *) {
    return sihttp_response({ .body = strdup("OK") });
}

void init_rest(ecs_world_t *world) {
    sihttp_app_state_t *state = malloc(sizeof(sihttp_app_state_t));

    state->world = world;

    world->server = sihttp_server(
        {
            .port = 4040,
            .state = state,
        }
    );

    sihttp_get(world->server, "/schema", ecs_rest_get_schema);
    sihttp_get(world->server, "/entities", ecs_rest_get_entities);
    sihttp_post(world->server, "/entities", ecs_rest_post_entities);
    sihttp_get(world->server, "/entities/:index/children", ecs_rest_get_entity_children);
    sihttp_get(world->server, "/health", health);
    sihttp_put(
        world->server,
        "/entities/:index/components/:component",
        ecs_rest_put_entity_component
    );
    sihttp_get(world->server, "/entities/:index", ecs_rest_get_entity);

    if (world->features.rest) {
        sihttp_server_start(world->server);
    }
}

sihttp_response_t ecs_rest_json_response(int status, sijson_value_t body) {
    char *json = sijson_stringify(body);
    if (!json) {
        json = strdup("{\"error\":\"failed to serialize response\"}");
        status = 500;
    }

    return sihttp_response({
        .status = status,
        .body = json,
        .content_type = SIHTTP_CONTENT_JSON,
    });
}

sihttp_response_t ecs_rest_error_response(int status, const char *message) {
    sijson_clean();

    sijson_value_t body = sijson_make_object();
    sijson_object_set(body, "error", sijson_make_string(message));

    return ecs_rest_json_response(status, body);
}

#ifndef SIJSON_H
#include "sijson.h"
#endif

static bool entity_from_index(ecs_world_t *world, int64_t index, ecs_entity_t *out) {
    if (index <= 0 || (uint64_t)index >= world->entity_index.entities.size) {
        return false;
    }

    ecs_entity_record_t *record =
        ecs_vec_get_mut(&world->entity_index.entities, (uint32_t)index, ecs_entity_record_t);
    if (record->table_id == UINT16_MAX) {
        return false;
    }

    *out = ecs_entity((uint32_t)index, record->generation);
    return true;
}

static bool entity_is_alive(ecs_world_t *world, ecs_entity_t entity) {
    ecs_entity_t current = 0;
    return entity_from_index(world, ecs_first(entity), &current) && current == entity;
}

static char *entity_name(ecs_world_t *world, ecs_entity_t entity) {
    if (ecs_has(world, entity, Name)) {
        const char *value = ecs_get(world, entity, Name)->value;
        return strdup(value ? value : "");
    }
    return siformat("(%d, %d)", ecs_first(entity), ecs_second(entity));
}

sijson_value_t ecs_rest_entity_json(ecs_world_t *world, ecs_entity_t entity) {
    sijson_value_t object = sijson_make_object();

    char *name = entity_name(world, entity);
    sijson_object_set(object, "name", sijson_make_string(name));
    free(name);

    sijson_object_set(object, "index", sijson_make_number(ecs_first(entity)));
    sijson_object_set(object, "generation", sijson_make_number(ecs_second(entity)));
    sijson_object_set(
        object,
        "hasChildren",
        sijson_make_bool(ecs_has_cid(world, entity, ecs_source(ChildOf)))
    );
    return object;
}

sijson_value_t ecs_rest_entity_children_json(ecs_world_t *world, ecs_entity_t entity) {
    sijson_value_t children = sijson_make_array();
    RelationSource *source = ecs_try_get_cid(world, entity, ecs_source(ChildOf));
    if (!source) {
        return children;
    }

    for (uint32_t i = 0; i < source->entities.size; i++) {
        ecs_entity_t child = *ecs_vec_get(&source->entities, i, ecs_entity_t);
        if (entity_is_alive(world, child)) {
            sijson_array_push(children, ecs_rest_entity_json(world, child));
        }
    }
    return children;
}

sijson_value_t ecs_rest_entity_detail_json(ecs_world_t *world, ecs_entity_t entity) {
    sijson_value_t detail = sijson_make_object();

    char *name = entity_name(world, entity);
    sijson_object_set(detail, "name", sijson_make_string(name));
    free(name);

    sijson_object_set(detail, "index", sijson_make_number(ecs_first(entity)));
    sijson_object_set(detail, "generation", sijson_make_number(ecs_second(entity)));

    ChildOf *parent = ecs_try_get(world, entity, ChildOf);
    if (parent) {
        sijson_object_set(detail, "parent", ecs_rest_entity_json(world, parent->target));
    }

    sijson_value_t components = sijson_make_array();
    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);
    for (uint32_t i = 0; i < table->type.count; i++) {
        ecs_component_t cid = table->type.ids[i];
        if (ecs_rest_entity_component_is_reflected(world, cid)) {
            void *ptr = ecs_table_get_component(table, cid, record->table_row);
            sijson_array_push(components, ecs_rest_entity_component_json(world, cid, ptr));
        }
    }

    sijson_object_set(detail, "children", ecs_rest_entity_children_json(world, entity));
    sijson_object_set(detail, "components", components);
    return detail;
}

sihttp_response_t ecs_rest_get_entities(const sihttp_request_t *req) {
    ecs_world_t *world = req->state->world;
    sijson_clean();

    sijson_value_t array = sijson_make_array();
    ecs_query_each(world, it, i, { ecs_source(ChildOf) }, { ecs_id(ChildOf), EcsNot }) {
        sijson_array_push(array, ecs_rest_entity_json(world, it.entities[i]));
    }
    ecs_query_each(world, it, i, { ecs_source(ChildOf), EcsNot }, { ecs_id(ChildOf), EcsNot }) {
        sijson_array_push(array, ecs_rest_entity_json(world, it.entities[i]));
    }
    return ecs_rest_json_response(200, array);
}

sihttp_response_t ecs_rest_get_entity(const sihttp_request_t *req) {
    ecs_world_t *world = req->state->world;
    sijson_clean();

    ecs_entity_t entity = 0;
    if (!entity_from_index(world, sihttp_param(req, "index"), &entity)) {
        return ecs_rest_error_response(404, "entity not found");
    }
    return ecs_rest_json_response(200, ecs_rest_entity_detail_json(world, entity));
}

sihttp_response_t ecs_rest_get_entity_children(const sihttp_request_t *req) {
    ecs_world_t *world = req->state->world;
    sijson_clean();

    ecs_entity_t entity = 0;
    if (!entity_from_index(world, sihttp_param(req, "index"), &entity)) {
        return ecs_rest_error_response(404, "entity not found");
    }
    return ecs_rest_json_response(200, ecs_rest_entity_children_json(world, entity));
}

sihttp_response_t ecs_rest_put_entity_component(const sihttp_request_t *req) {
    ecs_world_t *world = req->state->world;
    int64_t component = sihttp_param(req, "component");

    ecs_entity_t entity = 0;
    if (!entity_from_index(world, sihttp_param(req, "index"), &entity)) {
        sijson_clean();
        return ecs_rest_error_response(404, "entity not found");
    }
    if (component <= 0 || component > UINT16_MAX) {
        sijson_clean();
        return ecs_rest_error_response(404, "component not found");
    }

    return ecs_rest_set_entity_component(world, entity, (ecs_component_t)component, req->body);
}

sihttp_response_t ecs_rest_post_entities(const sihttp_request_t *req) {
    ecs_entity_t entity = ecs_new(req->state->world);
    return sihttp_response(
        {
            .body = sijson_stringify(ecs_rest_entity_json(req->state->world, entity)),
        }
    );
}

#ifndef SIJSON_H
#include "sijson.h"
#endif

static void ensure_sijson_entity_type(void) {
    sireflect_register_struct(
        sijson_default_registry(),
        &(sireflect_struct_desc_t){
            .name = "ecs_entity_t",
            .fields = "{ uint32_t id; uint32_t generation; }",
            .size = sizeof(ecs_entity_t),
            .align = _Alignof(ecs_entity_t),
        }
    );
}

bool ecs_rest_entity_component_is_reflected(ecs_world_t *world, ecs_component_t component) {
    if (component >= world->component_index.components.size) {
        return false;
    }

    const ecs_component_record_t *record =
        ecs_component_index_get(&world->component_index, component);
    return record->registered && record->reflection != SIREFLECT_INVALID_HANDLE &&
           record->reflection_desc != NULL;
}

static bool validate_component_shape(
    ecs_world_t *world,
    const ecs_component_record_t *record,
    sijson_value_t value
) {
    const sireflect_fields_t *fields =
        sireflect_type_fields(world->sireflect_registry, record->reflection);
    if (sijson_type(value) != SIJSON_OBJECT || sijson_object_len(value) != fields->field_count) {
        return false;
    }

    for (size_t i = 0; i < fields->field_count; i++) {
        if (!sijson_object_get(value, fields->fields[i].name)) {
            return false;
        }
    }

    for (size_t i = 0; i < sijson_object_len(value); i++) {
        const char *key = sijson_object_key(value, i);
        bool found = false;
        for (size_t f = 0; f < fields->field_count; f++) {
            found = found || strcmp(key, fields->fields[f].name) == 0;
        }
        if (!found) {
            return false;
        }
    }

    return true;
}

static sijson_value_t component_value_json(const ecs_component_record_t *record, const void *ptr) {
    ensure_sijson_entity_type();

    sireflect_handle_t ref = SIREFLECT_INVALID_HANDLE;
    char *json = sijson_to_json_impl(&ref, record->reflection_desc, ptr);
    if (!json) {
        return sijson_make_null();
    }

    sijson_value_t value = sijson_parse(json);
    free(json);
    return value ? value : sijson_make_null();
}

sijson_value_t
ecs_rest_entity_component_json(ecs_world_t *world, ecs_component_t component_id, const void *ptr) {
    const ecs_component_record_t *record =
        ecs_component_index_get(&world->component_index, component_id);
    const sireflect_type_info_t *type =
        sireflect_type_info(world->sireflect_registry, record->reflection);

    sijson_value_t component = sijson_make_object();
    sijson_object_set(component, "id", sijson_make_number(component_id));
    sijson_object_set(component, "name", sijson_make_string(type && type->name ? type->name : ""));
    sijson_object_set(component, "value", component_value_json(record, ptr));
    return component;
}

sihttp_response_t ecs_rest_set_entity_component(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t component,
    const char *body_text
) {
    sijson_clean();

    if (!ecs_is_alive(world, entity)) {
        return ecs_rest_error_response(404, "entity not found");
    }
    if (!ecs_rest_entity_component_is_reflected(world, component)) {
        return ecs_rest_error_response(404, "component not found");
    }
    if (!ecs_has_cid(world, entity, component)) {
        return ecs_rest_error_response(404, "entity component not found");
    }

    sijson_value_t body = sijson_parse(body_text);
    sijson_value_t value = body ? sijson_object_get(body, "value") : NULL;
    if (!body || sijson_type(body) != SIJSON_OBJECT || sijson_object_len(body) != 1 || !value) {
        return ecs_rest_error_response(400, "invalid json body");
    }

    const ecs_component_record_t *record =
        ecs_component_index_get(&world->component_index, component);
    if (!validate_component_shape(world, record, value)) {
        return ecs_rest_error_response(400, "invalid component value");
    }

    char *json = sijson_stringify(value);
    sireflect_handle_t ref = SIREFLECT_INVALID_HANDLE;
    void *decoded = json ? sijson_from_json_impl(&ref, record->reflection_desc, json) : NULL;
    free(json);
    if (!decoded || sijson_error()) {
        return ecs_rest_error_response(400, "invalid component value");
    }

    ecs_set_cid(world, entity, component, decoded);
    return ecs_rest_json_response(
        200,
        ecs_rest_entity_component_json(world, component, ecs_get_cid(world, entity, component))
    );
}

#include <stdbool.h>

typedef struct {
    bool *items;
    size_t count;
} ecs_rest_type_set_t;

static bool ecs_rest_component_is_reflected(ecs_world_t *world, ecs_component_t id) {
    if (id >= world->component_index.components.size) {
        return false;
    }

    const ecs_component_record_t *record = ecs_component_index_get(&world->component_index, id);
    return record->registered && record->reflection != SIREFLECT_INVALID_HANDLE;
}

static sijson_value_t ecs_rest_field_json(const sireflect_field_info_t *field) {
    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "name", sijson_make_string(field->name));
    sijson_object_set(object, "type", sijson_make_number(field->type));

    return object;
}

static sijson_value_t ecs_rest_component_json(
    ecs_world_t *world,
    ecs_component_t id,
    const ecs_component_record_t *record
) {
    sijson_value_t fields_json = sijson_make_array();
    const sireflect_type_info_t *type =
        sireflect_type_info(world->sireflect_registry, record->reflection);
    const sireflect_fields_t *fields =
        sireflect_type_fields(world->sireflect_registry, record->reflection);
    for (size_t i = 0; i < fields->field_count; i++) {
        sijson_array_push(fields_json, ecs_rest_field_json(&fields->fields[i]));
    }

    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "id", sijson_make_number(id));
    sijson_object_set(object, "name", sijson_make_string(type && type->name ? type->name : ""));
    sijson_object_set(object, "isRelation", sijson_make_bool(record->relation_flags != 0));
    sijson_object_set(object, "type", sijson_make_number(record->reflection));
    sijson_object_set(object, "fields", fields_json);

    return object;
}

static void ecs_rest_type_set_add(ecs_rest_type_set_t *set, sireflect_handle_t id) {
    if (id == SIREFLECT_INVALID_HANDLE) {
        return;
    }

    if (id >= set->count) {
        size_t count = set->count == 0 ? 64 : set->count;
        while (id >= count) {
            count *= 2;
        }

        bool *items = realloc(set->items, count * sizeof(bool));
        if (!items) {
            abort();
        }

        memset(items + set->count, 0, (count - set->count) * sizeof(bool));
        set->items = items;
        set->count = count;
    }

    set->items[id] = true;
}

static void ecs_rest_collect_component_types(
    ecs_world_t *world,
    ecs_rest_type_set_t *set,
    const ecs_component_record_t *record
) {
    ecs_rest_type_set_add(set, record->reflection);

    const sireflect_fields_t *fields =
        sireflect_type_fields(world->sireflect_registry, record->reflection);
    for (size_t i = 0; i < fields->field_count; i++) {
        ecs_rest_type_set_add(set, fields->fields[i].type);
    }
}

static bool ecs_rest_type_name_is(const sireflect_type_info_t *type, const char *name) {
    return type->name && strcmp(type->name, name) == 0;
}

static const char *
ecs_rest_editor_type(ecs_world_t *world, sireflect_handle_t id, const sireflect_type_info_t *type) {
    if (ecs_rest_type_name_is(type, "ecs_entity_t")) {
        return "entity";
    }

    if (type->kind == sireflect_kind_bool) {
        return "boolean";
    }

    if (sireflect_is_numeric(type->kind)) {
        return "number";
    }

    if (type->kind == sireflect_kind_struct) {
        return "object";
    }

    if (type->kind == sireflect_kind_pointer) {
        const sireflect_type_info_t *element =
            sireflect_type_info(world->sireflect_registry, type->element_type);
        if (element && element->kind == sireflect_kind_char) {
            return "string";
        }
    }

    (void)id;
    return "unsupported";
}

static sijson_value_t ecs_rest_type_json(ecs_world_t *world, sireflect_handle_t id) {
    const sireflect_type_info_t *type = sireflect_type_info(world->sireflect_registry, id);

    sijson_value_t object = sijson_make_object();
    sijson_object_set(object, "id", sijson_make_number(id));
    sijson_object_set(object, "name", sijson_make_string(type->name ? type->name : ""));
    sijson_object_set(object, "editor", sijson_make_string(ecs_rest_editor_type(world, id, type)));

    return object;
}

sihttp_response_t ecs_rest_get_schema(const sihttp_request_t *req) {
    ecs_world_t *world = req->state->world;
    ecs_rest_type_set_t types = { 0 };

    sijson_clean();

    sijson_value_t components = sijson_make_array();
    for (uint32_t i = 2; i < world->component_index.components.size; i++) {
        if (!ecs_rest_component_is_reflected(world, (ecs_component_t)i)) {
            continue;
        }

        const ecs_component_record_t *record =
            ecs_component_index_get(&world->component_index, (ecs_component_t)i);
        ecs_rest_collect_component_types(world, &types, record);
        sijson_array_push(components, ecs_rest_component_json(world, (ecs_component_t)i, record));
    }

    sijson_value_t type_values = sijson_make_array();
    for (size_t i = 1; i < types.count; i++) {
        if (types.items[i]) {
            sijson_array_push(type_values, ecs_rest_type_json(world, (sireflect_handle_t)i));
        }
    }

    free(types.items);

    sijson_value_t schema = sijson_make_object();
    sijson_object_set(schema, "components", components);
    sijson_object_set(schema, "types", type_values);

    return ecs_rest_json_response(200, schema);
}

#ifndef ECS_ARENA_H
#define ECS_ARENA_H

#include <stdint.h>
#include <stdlib.h>

typedef struct {
    uint32_t capacity;
    uint32_t cursor;
    uint8_t *buf;
} ecs_arena_t;

void ecs_arena_init(ecs_arena_t *allocator);
void ecs_arena_fini(ecs_arena_t *allocator);

static inline void *ecs_arena_alloc(ecs_arena_t *allocator, uint32_t size) {
    if (ECS_LIKELY(allocator->cursor + size <= allocator->capacity)) {
        allocator->cursor += size;
        return allocator->buf + (allocator->cursor - size);
    }
    allocator->buf = (uint8_t *)realloc(allocator->buf, allocator->capacity + size);
    allocator->capacity = allocator->capacity + size;
    allocator->cursor += size;
    return allocator->buf + (allocator->cursor - size);
}

static inline void ecs_arena_reset(ecs_arena_t *allocator) { allocator->cursor = 0; }

#endif

void ecs_arena_init(ecs_arena_t *allocator) {
    allocator->buf = malloc(8);
    allocator->capacity = 8;
    allocator->cursor = 0;
}
void ecs_arena_fini(ecs_arena_t *allocator) {
    free(allocator->buf);
}

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
void ecs_str_cstr_append(ecs_str_t *dst, const char *src);
void ecs_str_insert(ecs_str_t *str, uint32_t pos, char c);
void ecs_str_remove(ecs_str_t *str, uint32_t pos);
void ecs_str_pop_back(ecs_str_t *str);

void ecs_str_trim(ecs_str_t *str);

bool ecs_str_starts_with(const ecs_str_t *str, const ecs_str_t *prefix);
bool ecs_str_ends_with(const ecs_str_t *str, const ecs_str_t *suffix);
bool ecs_str_cmp(const ecs_str_t *a, const ecs_str_t *b);

#endif

#include <ctype.h>

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

void ecs_str_cstr_append(ecs_str_t *dst, const char *src) {
    if (!src || *src == '\0')
        return;
    uint32_t required = dst->len + strlen(src);
    if (required > dst->capacity) {
        ecs_str_reserve(dst, required);
    }
    memcpy(dst->data + dst->len, src, strlen(src));
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

void ecs_vec_init(ecs_vec_t *vec, uint32_t element_size) {
    vec->data = malloc(element_size); // Start with 1 elements
    vec->size = 0;
    vec->capacity = 1;
}

void ecs_vec_init_w_size(ecs_vec_t *vec, uint32_t element_size, uint32_t size) {
    vec->data = malloc(element_size * size);
    vec->size = 0;
    vec->capacity = size;
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

#ifndef ECS_PARSING_SCANNER_H
#define ECS_PARSING_SCANNER_H

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

void ecs_scanner_init(ecs_scanner_t *scanner, const char *str) {
    scanner->str = str;
    scanner->pos = 0;
    scanner->len = (uint32_t)strlen(str);
}

#ifndef SIREFLECT_H
#include "sireflect.h"
#endif

void ecs_component_index_register(
    ecs_component_index_t *index,
    ecs_component_t id,
    uint64_t size,
    ecs_component_on_set_t on_set,
    ecs_component_on_remove_t on_remove,
    ecs_component_on_add_t on_add,
    uint32_t relation_flags,
    sireflect_handle_t reflection,
    const sireflect_struct_desc_t *reflection_desc
) {
    ecs_vec_ensure(&index->components, (uint32_t)id + 1, sizeof(ecs_component_record_t));

    ecs_component_record_t *existing =
        ecs_vec_get_mut(&index->components, id, ecs_component_record_t);
    if (existing->registered) {
        return;
    }

    ecs_component_record_t record = {
        .registered = true,
        .required = NULL,
        .required_count = 0,
        .size = size,
        .on_set = on_set,
        .on_remove = on_remove,
        .on_add = on_add,
        .relation_flags = relation_flags,
        .tables = { 0 },
        .reflection = reflection,
        .reflection_desc = reflection_desc,
    };
    ecs_vec_init(&record.tables, sizeof(uint16_t));

    *existing = record;
}

void ecs_component_index_init(ecs_component_index_t *index) {
    ecs_vec_init_w_size(&index->components, sizeof(ecs_component_record_t), 256);
}

void ecs_component_index_fini(ecs_component_index_t *index) {
    ecs_component_record_t *records = index->components.data;

    for (uint32_t i = 0; i < index->components.size; i++) {
        if (!records[i].registered) {
            continue;
        }
        free(records[i].required);
        ecs_vec_fini(&records[i].tables);
    }
    ecs_vec_fini(&index->components);
}

void ecs_entity_index_init(ecs_entity_index_t *index) {
    ecs_vec_init_w_size(&index->entities, sizeof(ecs_entity_record_t), 256);
    index->first_available = UINT32_MAX;
}

void ecs_entity_index_fini(ecs_entity_index_t *index) {
    ecs_vec_fini(&index->entities);
}

static bool ecs_module_id_valid(const ecs_module_index_t *index, ecs_module_id_t module) {
    return module != 0 && module < index->modules.size;
}

static void ecs_module_record_init(ecs_module_t *module, ecs_module_id_t *id, const char *name) {
    module->id = id;
    module->name = name;
    module->enabled = true;
    ecs_vec_init(&module->observers, sizeof(ecs_observer_id_t));
    ecs_vec_init(&module->systems, sizeof(ecs_system_id_t));
    ecs_vec_init(&module->components, sizeof(ecs_component_t));
}

static void ecs_module_record_fini(ecs_module_t *module) {
    if (module->id) {
        *module->id = 0;
    }

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
    ecs_module_id_t *id,
    const char *name
) {
    ecs_module_t module;
    ecs_module_record_init(&module, id, name);
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

ecs_module_id_t ecs_module_index_find(const ecs_module_index_t *index, const ecs_module_id_t *id) {
    if (!id || !*id) {
        return 0;
    }

    ecs_module_id_t module = *id;
    if (ecs_module_id_valid(index, module)) {
        return module;
    }

    return 0;
}

#define ECS_BUILTIN_EVENT_COUNT 3 // EcsOnAdd, EcsOnRemove, EcsOnSet

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
    obs->enabled = true;
    ecs_query_from_desc(&desc->query, &obs->query);
    return index->observers.size - 1;
}

void ecs_observer_index_match_tables(
    ecs_world_t *world,
    ecs_table_t *tables,
    uint16_t table_count,
    uint16_t observer_id
) {
    ecs_observer_t *obs =
        ecs_vec_get_mut(&world->observer_index.observers, observer_id, ecs_observer_t);
    for (uint16_t i = 0; i < table_count; i++) {
        if (ecs_query_match_table(world, &obs->query, &tables[i])) {
            ecs_table_add_observer(&tables[i], obs->event, observer_id);
        }
    }
}

void ecs_observer_index_add_table(ecs_world_t *world, ecs_table_t *table) {
    for (uint32_t i = 0; i < world->observer_index.observers.size; i++) {
        ecs_observer_t *obs = ecs_vec_get_mut(&world->observer_index.observers, i, ecs_observer_t);
        if (ecs_query_match_table(world, &obs->query, table)) {
            ecs_table_add_observer(table, obs->event, i);
        }
    }
}

void ecs_query_index_init(ecs_query_index_t *index) {
    ecs_vec_init(&index->queries, sizeof(ecs_query_cache_t));
    ecs_vec_init(&index->active_ids, sizeof(ecs_query_id_t));
    index->first_free = UINT16_MAX;
}

void ecs_query_index_fini(ecs_query_index_t *index) {
    const ecs_query_id_t *active_ids = index->active_ids.data;
    for (uint32_t i = 0; i < index->active_ids.size; i++) {
        ecs_query_cache_t *cache =
            ecs_vec_get_mut(&index->queries, active_ids[i], ecs_query_cache_t);
        ecs_vec_fini(&cache->table_ids);
        free(cache->fields_ptr);
        free(cache->fields_kind);
        ecs_query_index_destroy(&cache->query);
    }
    ecs_vec_fini(&index->active_ids);
    ecs_vec_fini(&index->queries);
}

void ecs_query_index_destroy(ecs_query_t *query) {
    free(query->terms);
    free(query->fields);
}

static uint16_t ecs_query_count_terms(const ecs_query_term_t *terms) {
    uint16_t i = 0;
    while (terms[i].id) {
        i++;
    }
    return i;
}

static ecs_query_term_t *ecs_query_copy_terms(const ecs_query_term_t *terms, uint16_t count) {
    if (count == 0) {
        return NULL;
    }
    ecs_query_term_t *copy = malloc(sizeof(ecs_query_term_t) * count);
    memcpy(copy, terms, sizeof(ecs_query_term_t) * count);
    return copy;
}

static bool
ecs_query_has_term(const ecs_query_term_t *terms, uint16_t term_count, ecs_component_t id) {
    for (uint16_t i = 0; i < term_count; i++) {
        if (terms[i].id == id) {
            return true;
        }
    }
    return false;
}

static ecs_query_term_t *
ecs_query_copy_terms_with_implicit_excludes(const ecs_query_term_t *terms, uint16_t *count) {
    const ecs_component_t excludes[] = {
        ecs_id(Disabled),
        ecs_id(Abstract),
    };
    const uint16_t explicit_count = *count;
    uint16_t exclude_count = 0;

    for (uint32_t i = 0; i < sizeof(excludes) / sizeof(excludes[0]); i++) {
        if (excludes[i] && !ecs_query_has_term(terms, explicit_count, excludes[i])) {
            exclude_count++;
        }
    }

    if (exclude_count == 0) {
        return ecs_query_copy_terms(terms, *count);
    }

    ecs_assert(*count + exclude_count < 16, "query has no room for implicit exclude terms\n");

    ecs_query_term_t *copy = malloc(sizeof(ecs_query_term_t) * (*count + exclude_count));
    if (*count != 0) {
        memcpy(copy, terms, sizeof(ecs_query_term_t) * *count);
    }

    for (uint32_t i = 0; i < sizeof(excludes) / sizeof(excludes[0]); i++) {
        if (excludes[i] && !ecs_query_has_term(terms, explicit_count, excludes[i])) {
            copy[*count] = (ecs_query_term_t){ .id = excludes[i], .access = EcsNot };
            *count += 1;
        }
    }

    return copy;
}

static bool ecs_query_term_is_field(ecs_query_term_t term) {
    return term.access == EcsIn || term.access == EcsOut || term.access == EcsInOut ||
           term.access == EcsInOptional || term.access == EcsInOutOptional;
}

static bool ecs_query_term_is_positive(ecs_query_term_t term) {
    return term.access == EcsIn || term.access == EcsOut || term.access == EcsInOut ||
           term.access == EcsFilter;
}

static void ecs_query_validate_terms(const ecs_query_term_t *terms, uint16_t term_count) {
    for (uint16_t i = 0; i < term_count; i++) {
        ecs_assert_id_valid(terms[i].id);
        ecs_assert(
            terms[i].access == EcsIn || terms[i].access == EcsOut || terms[i].access == EcsInOut ||
                terms[i].access == EcsInOptional || terms[i].access == EcsInOutOptional ||
                terms[i].access == EcsFilter || terms[i].access == EcsNot,
            "invalid query term access: %d\n",
            terms[i].access
        );

        for (uint16_t j = i + 1; j < term_count; j++) {
            ecs_assert(
                terms[i].id != terms[j].id,
                "duplicate query term component: %d\n",
                terms[i].id
            );
        }
    }
}

void ecs_query_from_desc(const ecs_query_desc_t *desc, ecs_query_t *query) {
    query->term_count = ecs_query_count_terms(desc->terms);
    ecs_assert(
        query->term_count != 0 || desc->is_a != 0,
        "query must contain at least one term or is_a target\n"
    );

    query->terms = ecs_query_copy_terms_with_implicit_excludes(desc->terms, &query->term_count);
    ecs_query_validate_terms(query->terms, query->term_count);

    query->is_a = desc->is_a;

    query->field_count = 0;
    for (uint16_t i = 0; i < query->term_count; i++) {
        if (ecs_query_term_is_field(query->terms[i])) {
            query->field_count++;
        }
    }

    query->fields = NULL;
    if (query->field_count != 0) {
        query->fields = malloc(sizeof(ecs_query_term_t) * query->field_count);

        uint16_t field = 0;
        for (uint16_t i = 0; i < query->term_count; i++) {
            if (ecs_query_term_is_field(query->terms[i])) {
                query->fields[field++] = query->terms[i];
            }
        }
    }

    query->bloom = 0;
    for (uint16_t i = 0; i < query->term_count; i++) {
        if (ecs_query_term_is_positive(query->terms[i])) {
            query->bloom |= 1ull << (query->terms[i].id % 64);
        }
    }
}

static void ecs_query_cache_add_table(
    ecs_world_t *world,
    ecs_query_cache_t *cache,
    const ecs_table_t *table,
    uint16_t table_id
) {
    ecs_vec_push_u16(&cache->table_ids, table_id);
    const uint16_t table_count = cache->table_ids.size;
    const uint16_t field_count = cache->query.field_count;

    if (table_count > cache->field_table_capacity) {
        uint16_t capacity = cache->field_table_capacity ? cache->field_table_capacity : 4;
        while (capacity < table_count) {
            capacity *= 2;
        }

        const uint32_t slot_count = (uint32_t)capacity * field_count;
        cache->fields_ptr = realloc(cache->fields_ptr, sizeof(void *) * slot_count);
        cache->fields_kind = realloc(cache->fields_kind, sizeof(ecs_field_kind_t) * slot_count);
        cache->field_table_capacity = capacity;
    }

    const uint32_t base = (uint32_t)(table_count - 1) * field_count;
    for (uint16_t i = 0; i < cache->query.field_count; i++) {
        const ecs_query_term_t term = cache->query.fields[i];
        void *field = NULL;
        ecs_field_kind_t field_kind = EcsFieldNone;

        if (ecs_query_term_requires_owned(term)) {
            uint16_t column = ecs_table_column_or_invalid(table, term.id);
            if (column != UINT16_MAX) {
                field = &table->cls[column].data;
                field_kind = EcsFieldOwned;
            }
        } else {
            bool is_shared = false;
            field = ecs_table_field(world, table, term.id, &is_shared);
            if (field || is_shared) {
                field_kind = is_shared ? EcsFieldShared : EcsFieldOwned;
            }
        }

        ecs_assert(
            field_kind != EcsFieldNone || term.access == EcsInOptional ||
                term.access == EcsInOutOptional,
            "query cache matched table without field component: %d\n",
            term.id
        );

        cache->fields_ptr[base + i] = field;
        cache->fields_kind[base + i] = field_kind;
    }
}

ecs_query_id_t ecs_query_index_create(ecs_query_index_t *index, const ecs_query_desc_t *desc) {
    ecs_query_id_t id;
    ecs_query_cache_t *query_cache;

    if (index->first_free != UINT16_MAX) {
        id = index->first_free;
        query_cache = ecs_vec_get_mut(&index->queries, id, ecs_query_cache_t);
        index->first_free = query_cache->next_free;
    } else {
        query_cache = ecs_vec_push_empty(&index->queries, sizeof(ecs_query_cache_t));
        id = index->queries.size - 1;
    }

    ecs_query_from_desc(desc, &query_cache->query);
    ecs_vec_init(&query_cache->table_ids, sizeof(uint16_t));
    query_cache->fields_ptr = NULL;
    query_cache->fields_kind = NULL;
    query_cache->field_table_capacity = 0;
    query_cache->active_index = index->active_ids.size;
    query_cache->next_free = UINT16_MAX;
    query_cache->alive = true;
    ecs_vec_push_u16(&index->active_ids, id);

    return id;
}

static ecs_component_t ecs_query_first_positive_term(const ecs_query_t *query) {
    for (uint16_t i = 0; i < query->term_count; i++) {
        if (ecs_query_term_is_positive(query->terms[i])) {
            return query->terms[i].id;
        }
    }
    return 0;
}

void ecs_query_index_update_matches(ecs_world_t *world, ecs_query_cache_t *query_cache) {
    uint16_t component = ecs_query_first_positive_term(&query_cache->query);

    if (ECS_LIKELY(component)) {
        const ecs_vec_t *tables_vec =
            &ecs_component_index_get(&world->component_index, component)->tables;

        ecs_vec_iter(tables_vec, uint16_t, table_index, {
            const ecs_table_t *table = &world->table_index.tables[*table_index];

            if (ecs_query_match_table(world, &query_cache->query, table)) {
                ecs_query_cache_add_table(world, query_cache, table, *table_index);
            }
        });
    } else {
        const uint16_t table_count = world->table_index.table_count;
        const ecs_table_t *tables = world->table_index.tables;

        for (uint16_t i = 0; i < table_count; i++) {
            if (ecs_query_match_table(world, &query_cache->query, &tables[i])) {
                ecs_query_cache_add_table(world, query_cache, &tables[i], i);
            }
        }
    }
}

void ecs_query_index_add_table(ecs_world_t *world, const ecs_table_t *table, uint16_t table_id) {
    const ecs_query_id_t *active_ids = world->query_index.active_ids.data;
    for (uint32_t i = 0; i < world->query_index.active_ids.size; i++) {
        ecs_query_cache_t *cache =
            ecs_vec_get_mut(&world->query_index.queries, active_ids[i], ecs_query_cache_t);
        if (ecs_query_match_table(world, &cache->query, table)) {
            ecs_query_cache_add_table(world, cache, table, table_id);
        }
    }
}

static uint64_t ecs_resource_storage_size(const ecs_resource_desc_t *record) {
    return record->size ? record->size : 1;
}

static void
ecs_resource_index_assert_registered(const ecs_resource_index_t *index, ecs_resource_t id) {
    ecs_assert(
        id != 0 && id < index->count && id < index->capacity && index->records[id].name != NULL,
        "invalid resource id: %d\n",
        id
    );
}

static void ecs_resource_index_ensure(ecs_resource_index_t *index, ecs_resource_t id) {
    if (id < index->capacity) {
        return;
    }

    uint64_t capacity = index->capacity ? index->capacity : 16;
    while (id >= capacity) {
        capacity *= 2;
    }

    ecs_resource_desc_t *records = realloc(index->records, sizeof(ecs_resource_desc_t) * capacity);
    ecs_assert_not_null(records);
    void **data = realloc(index->data, sizeof(void *) * capacity);
    ecs_assert_not_null(data);
    bool *present = realloc(index->present, sizeof(bool) * capacity);
    ecs_assert_not_null(present);

    for (uint64_t i = index->capacity; i < capacity; i++) {
        records[i] = (ecs_resource_desc_t){ 0 };
        data[i] = NULL;
        present[i] = false;
    }

    index->records = records;
    index->data = data;
    index->present = present;
    index->capacity = capacity;
}

void ecs_resource_index_init(ecs_resource_index_t *index) {
    index->records = NULL;
    index->data = NULL;
    index->present = NULL;
    index->capacity = 0;
    index->count = 1;
}

void ecs_resource_index_fini(ecs_resource_index_t *index, ecs_world_t *world) {
    for (uint64_t id = 1; id < index->capacity; id++) {
        if (!index->present[id]) {
            continue;
        }

        const ecs_resource_desc_t *record = &index->records[id];
        if (record->on_remove) {
            record->on_remove(world, index->data[id]);
        }
        free(index->data[id]);
    }

    free(index->records);
    free(index->data);
    free(index->present);
}

ecs_resource_t
ecs_resource_index_register(ecs_resource_index_t *index, const ecs_resource_desc_t *desc) {
    ecs_assert_not_null(desc);
    ecs_assert_not_null(desc->name);

    ecs_resource_t id = (ecs_resource_t)index->count++;
    ecs_resource_index_ensure(index, id);
    index->records[id] = *desc;
    return id;
}

ecs_resource_t ecs_resource_index_find(const ecs_resource_index_t *index, const char *name) {
    ecs_assert_not_null(name);

    for (uint32_t id = 1; id < index->count; id++) {
        if (index->records[id].name && strcmp(index->records[id].name, name) == 0) {
            return id;
        }
    }

    return 0;
}

bool ecs_resource_index_is_registered(const ecs_resource_index_t *index, ecs_resource_t id) {
    return id != 0 && id < index->count && id < index->capacity && index->records[id].name != NULL;
}

void ecs_resource_index_set(
    ecs_resource_index_t *index,
    ecs_world_t *world,
    ecs_resource_t id,
    const void *data
) {
    ecs_resource_index_assert_registered(index, id);

    const ecs_resource_desc_t *record = &index->records[id];
    if (!index->present[id]) {
        index->data[id] = calloc(1, ecs_resource_storage_size(record));
        ecs_assert_not_null(index->data[id]);
        index->present[id] = true;
    }

    if (record->on_set) {
        record->on_set(world, data);
    }

    if (record->size != 0) {
        memcpy(index->data[id], data, record->size);
    }
}

void *ecs_resource_index_get(ecs_resource_index_t *index, ecs_resource_t id) {
    ecs_resource_index_assert_registered(index, id);
    if (!index->present[id]) {
        return NULL;
    }

    return index->data[id];
}

const void *ecs_resource_index_get_const(const ecs_resource_index_t *index, ecs_resource_t id) {
    ecs_resource_index_assert_registered(index, id);
    if (!index->present[id]) {
        return NULL;
    }

    return index->data[id];
}

bool ecs_resource_index_has(const ecs_resource_index_t *index, ecs_resource_t id) {
    ecs_resource_index_assert_registered(index, id);
    return index->present[id];
}

void ecs_resource_index_remove(ecs_resource_index_t *index, ecs_world_t *world, ecs_resource_t id) {
    ecs_resource_index_assert_registered(index, id);
    if (!index->present[id]) {
        return;
    }

    const ecs_resource_desc_t *record = &index->records[id];
    if (record->on_remove) {
        record->on_remove(world, index->data[id]);
    }

    free(index->data[id]);
    index->data[id] = NULL;
    index->present[id] = false;
}

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

#define INITIAL_SLOT_SHIFT 12
#define LOAD_FACTOR 0.75
#define ECS_TABLE_SLOT_EMPTY UINT16_MAX

static inline uint32_t ecs_type_hash(ecs_type_t type) {
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < type.count; ++i) {
        h ^= (uint32_t)type.ids[i];
        h *= 16777619u;
    }
    h ^= (uint32_t)type.base;
    h *= 16777619u;
    h ^= (uint32_t)(type.base >> 32);
    h *= 16777619u;

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
    memset(map->slots, 0xFF, sizeof(ecs_type_slot_t) * slot_count);
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

static bool ecs_table_index_inherits_component_before(
    const ecs_world_t *world,
    const ecs_table_t *table,
    ecs_entity_t stop_base,
    ecs_component_t component
) {
    ecs_entity_t base = table->type.base;
    while (base != 0 && base != stop_base) {
        const ecs_entity_record_t *record = ecs_get_record(world, base);
        const ecs_table_t *base_table = ecs_get_table(world, record->table_id);
        if (ecs_table_column_or_invalid(base_table, component) != UINT16_MAX) {
            return true;
        }
        base = base_table->type.base;
    }
    return false;
}

static void ecs_table_index_register_inherited_components(
    ecs_world_t *world,
    ecs_table_t *table,
    uint16_t table_id
) {
    ecs_entity_t base = table->type.base;
    while (base != 0) {
        const ecs_entity_record_t *record = ecs_get_record(world, base);
        const ecs_table_t *base_table = ecs_get_table(world, record->table_id);

        for (uint16_t i = 0; i < base_table->type.count; i++) {
            ecs_component_t component = base_table->type.ids[i];
            if (ecs_table_column_or_invalid(table, component) != UINT16_MAX ||
                ecs_table_index_inherits_component_before(world, table, base, component)) {
                continue;
            }

            table->bloom |= 1ull << (component % 64);
            ecs_component_record_t *record =
                ecs_component_index_get_mut(&world->component_index, component);
            ecs_vec_push_u16(&record->tables, table_id);
        }

        base = base_table->type.base;
    }
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
            if (ECS_LIKELY(ecs_type_equals(&table->type, &type))) {
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
    ecs_table_index_register_inherited_components(world, &map->tables[table_idx], table_idx);

    map->slots[slot_idx].hash = hash_fingerprint;
    map->slots[slot_idx].table_index = table_idx;

    ecs_query_index_add_table(world, ecs_table_index_at(map, table_idx), table_idx);
    ecs_observer_index_add_table(world, ecs_table_index_at(map, table_idx));
    return (uint16_t)table_idx;
}

