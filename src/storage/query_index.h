#ifndef SIECS_STORAGE_QUERY_INDEX_H
#define SIECS_STORAGE_QUERY_INDEX_H
#include "../helper.h"
#include "../table.h"
#include "siecs.h"
#include <stdint.h>

#define ECS_QUERY_RETAIN_TABLE_CAPACITY 8
#define ECS_QUERY_COMPILED_COMPONENT_CAPACITY \
    (ECS_QUERY_TERM_CAPACITY + ECS_QUERY_RELATION_CAPACITY + 2)

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

typedef struct {
    uint64_t bloom;
    ecs_entity_t is_a;
    ecs_query_order_t order_by;
    uint16_t component_term_count;
    uint16_t field_count;
    uint16_t field_mask;
    uint16_t up_mask;
    uint16_t filter_count;
    uint16_t component_access_count;
    uint16_t resource_access_count;
} ecs_query_t;

static inline ecs_access_t ecs_access_term_access(ecs_access_term_t term) {
    return (ecs_access_t)(term.access & UINT32_C(0xff));
}

static inline ecs_relation_id_t ecs_access_term_source_relation(ecs_access_term_t term) {
    return (ecs_relation_id_t)(term.access >> 8);
}

static inline bool ecs_query_desc_tracks_tables(const ecs_query_desc_t *desc) {
    return desc->components[0].id || desc->relations[0].id ||
           desc->order_by.func || desc->is_a;
}

typedef struct ecs_query_cache_s {
    ecs_query_t query;
    sicore_vec_t table_ids; // uint16_t
    void **fields_ptr;
    uint32_t *field_kind_bits;
    uint16_t field_table_capacity;
    uint32_t active_index;
    ecs_observer_id_t observer;
    uint16_t next_free;
    bool alive;
    ecs_component_term_t component_terms[ECS_QUERY_COMPILED_COMPONENT_CAPACITY];
    ecs_query_type_filter_t filters[ECS_QUERY_RELATION_CAPACITY];
    uint32_t component_accesses[ECS_QUERY_TERM_CAPACITY];
    uint32_t resource_accesses[ECS_QUERY_RESOURCE_CAPACITY];
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

static inline bool ecs_component_term_requires_owned(ecs_component_term_t term) {
    const ecs_access_t access = ecs_access_term_access(term);
    return access == EcsOut || access == EcsInOut || access == EcsInOutOptional;
}

static inline bool ecs_query_match_table(const ecs_query_cache_t *cache, const ecs_table_t *table) {
    const ecs_query_t *query = &cache->query;
    if (ECS_LIKELY((query->bloom & table->bloom) != query->bloom)) {
        return false;
    }
    if (query->is_a && !ecs_table_is_a(table, query->is_a)) {
        return false;
    }
    for (uint16_t i = 0; i < query->component_term_count; i++) {
        ecs_component_term_t term = cache->component_terms[i];
        ecs_access_t access = ecs_access_term_access(term);
        if (access == EcsInOptional || access == EcsInOutOptional || access == EcsInUp ||
            access == EcsInUpOptional) {
            continue;
        }
        if (access == EcsNot) {
            if (ecs_table_has(table, term.id)) {
                return false;
            }
        } else if (ecs_component_term_requires_owned(term)) {
            if (ecs_table_column_or_invalid(table, term.id) == UINT16_MAX) {
                return false;
            }
        } else if (!ecs_table_has(table, term.id)) {
            return false;
        }
    }
    for (uint16_t i = 0; i < query->filter_count; i++) {
        ecs_query_type_filter_t filter = cache->filters[i];
        uint16_t pair_index = ecs_type_pair_index(&table->type, filter.id);
        bool present = pair_index != UINT16_MAX;
        if ((filter.op == EcsQueryFilterRequired && !present) ||
            (filter.op == EcsQueryFilterExcluded && present) ||
            (filter.op == EcsQueryFilterExact &&
             (!present || ecs_type_pairs(&table->type)[pair_index].value != filter.value))) {
            return false;
        }
    }
    return true;
}
bool ecs_query_resolve_up_fields(
    ecs_query_cache_t *cache,
    const ecs_table_t *table,
    uint16_t table_index
);

#endif
