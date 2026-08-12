#ifndef SIECS_STORAGE_QUERY_INDEX_H
#define SIECS_STORAGE_QUERY_INDEX_H
#include "../helper.h"
#include "../table.h"
#include "siecs.h"
#include <stdint.h>

#define ECS_QUERY_RETAIN_TABLE_CAPACITY 8

typedef struct {
    uint64_t bloom;
    ecs_entity_t is_a;
    ecs_query_term_t *terms;
    uint16_t term_count;
    uint16_t field_count;
    uint16_t field_mask;
    uint16_t up_mask;
} ecs_query_t;

typedef struct {
    uint16_t filter_count;
    ecs_query_order_t order_by;
    uint32_t reserved;
} ecs_query_type_filter_meta_t;

#define ECS_QUERY_HAS_TYPE_FILTERS UINT16_C(0x8000)
#define ECS_QUERY_UP_MASK UINT16_C(0x7fff)

typedef enum {
    EcsQueryFilterRequired,
    EcsQueryFilterExcluded,
    EcsQueryFilterExact,
} ecs_query_filter_op_t;

typedef struct {
    uint64_t value;
    uint16_t id;
    uint8_t op;
} ecs_query_type_filter_t;

static inline ecs_term_access_t ecs_query_term_access(ecs_query_term_t term) {
    return (ecs_term_access_t)((uint32_t)term.access & UINT32_C(0xff));
}

static inline ecs_relation_id_t ecs_query_term_source_relation(ecs_query_term_t term) {
    return (ecs_relation_id_t)((uint32_t)term.access >> 8);
}

static inline ecs_query_type_filter_meta_t *ecs_query_type_filter_meta(const ecs_query_t *query) {
    uintptr_t end = (uintptr_t)(query->terms + query->term_count);
    return (ecs_query_type_filter_meta_t *)(
        (end + _Alignof(ecs_query_type_filter_meta_t) - 1) &
        ~(uintptr_t)(_Alignof(ecs_query_type_filter_meta_t) - 1)
    );
}

static inline ecs_query_type_filter_t *ecs_query_type_filters(const ecs_query_t *query) {
    return (ecs_query_type_filter_t *)(ecs_query_type_filter_meta(query) + 1);
}

typedef struct ecs_query_cache_s {
    ecs_query_t query;
    sicore_vec_t table_ids; // uint16_t
    void **fields_ptr;
    uint32_t *field_kind_bits;
    uint16_t field_table_capacity;
    uint16_t terms_capacity;
    uint32_t active_index;
    uint16_t next_free;
    bool alive;
} ecs_query_cache_t;

typedef struct {
    sicore_vec_t queries;
    sicore_vec_t active_ids; // ecs_query_id_t
    uint16_t first_free;
} ecs_query_index_t;

extern ecs_query_index_t query_index;

void ecs_query_index_init();
void ecs_query_index_fini();
uint16_t ecs_query_index_create(const ecs_query_desc_t *desc);
void ecs_query_index_add_table(const ecs_table_t *table, uint16_t table_id);
void ecs_query_index_refresh_table_fields(const ecs_table_t *table, uint16_t table_id);

// Reusable query helpers shared with the observer index.
ecs_component_t ecs_query_from_desc(const ecs_query_desc_t *desc, ecs_query_t *query);
void ecs_query_index_destroy(ecs_query_t *query);

static inline bool ecs_query_term_requires_owned(ecs_query_term_t term) {
    return term.access == EcsOut || term.access == EcsInOut || term.access == EcsInOutOptional;
}

static inline bool
ecs_query_match_component_table(const ecs_query_t *query, const ecs_table_t *table) {
    if (ECS_LIKELY((query->bloom & table->bloom) != query->bloom)) {
        return false;
    }
    if (query->is_a && !ecs_table_is_a(table, query->is_a)) {
        return false;
    }
    for (uint16_t i = 0; i < query->term_count; i++) {
        ecs_query_term_t term = query->terms[i];
        ecs_term_access_t access = term.access;
        if (access == EcsInOptional || access == EcsInOutOptional) {
            continue;
        }
        if (access == EcsNot) {
            if (ecs_table_has(table, term.id)) {
                return false;
            }
        } else if (ecs_query_term_requires_owned(term)) {
            if (ecs_table_column_or_invalid(table, term.id) == UINT16_MAX) {
                return false;
            }
        } else if (!ecs_table_has(table, term.id)) {
            return false;
        }
    }
    return true;
}

bool ecs_query_match_table(const ecs_query_t *query, const ecs_table_t *table);
bool ecs_query_resolve_up_fields(
    ecs_query_cache_t *cache,
    const ecs_table_t *table,
    uint16_t table_index
);

#endif
