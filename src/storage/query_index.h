#ifndef SIECS_STORAGE_QUERY_INDEX_H
#define SIECS_STORAGE_QUERY_INDEX_H
#include "../helper.h"
#include "../table.h"
#include "siecs.h"
#include <stdint.h>

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
    uint16_t required_field_mask;
    uint16_t up_mask;
    uint8_t field_count;
    uint8_t match_count;
    uint8_t filter_count;
    uint8_t resource_count;
    uint64_t data[];
} ecs_query_t;

static inline const ecs_component_term_t *ecs_query_fields(const ecs_query_t *query) {
    return (const ecs_component_term_t *)query->data;
}
static inline const ecs_component_term_t *ecs_query_match_terms(const ecs_query_t *query) {
    return ecs_query_fields(query) + query->field_count;
}
static inline const ecs_resource_term_t *ecs_query_resources(const ecs_query_t *query) {
    return (const ecs_resource_term_t *)(ecs_query_match_terms(query) + query->match_count);
}
static inline const ecs_query_type_filter_t *ecs_query_filters(const ecs_query_t *query) {
    return (const ecs_query_type_filter_t *)(ecs_query_resources(query) + query->resource_count);
}

static inline ecs_access_t ecs_access_term_access(ecs_access_term_t term) {
    return (ecs_access_t)(term.access & UINT32_C(0xff));
}

static inline ecs_relation_id_t ecs_access_term_source_relation(ecs_access_term_t term) {
    return (ecs_relation_id_t)(term.access >> 8);
}

typedef struct {
    uint32_t field_kind_bits;
    uint16_t id;
    uint16_t _padding;
    void *fields[];
} ecs_query_table_t;

typedef struct ecs_query_cache_s {
    ecs_query_t *query;
    uint8_t *tables;
    uint32_t active_index;
    ecs_observer_id_t observer;
    uint32_t table_capacity;
    uint16_t table_count;
    uint16_t next_free;
    bool alive;
} ecs_query_cache_t;

_Static_assert(sizeof(ecs_query_cache_t) <= 48, "ecs_query_cache_t must remain compact");

static inline size_t ecs_query_table_stride(const ecs_query_t *query) {
    return query->field_count
        ? sizeof(ecs_query_table_t) + (size_t)query->field_count * sizeof(void *)
        : sizeof(uint16_t);
}
static inline ecs_query_table_t *ecs_query_table_at(ecs_query_cache_t *cache, uint16_t index) {
    return (ecs_query_table_t *)(cache->tables + (size_t)index * ecs_query_table_stride(cache->query));
}
static inline const ecs_query_table_t *ecs_query_table_at_const(
    const ecs_query_cache_t *cache, uint16_t index
) {
    return (const ecs_query_table_t *)(cache->tables +
        (size_t)index * ecs_query_table_stride(cache->query));
}
static inline uint16_t ecs_query_table_id(const ecs_query_cache_t *cache, uint16_t index) {
    return cache->query->field_count
        ? ecs_query_table_at_const(cache, index)->id
        : ((const uint16_t *)cache->tables)[index];
}

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
bool ecs_query_resolve_up_fields(
    ecs_query_cache_t *cache,
    const ecs_table_t *table,
    ecs_query_table_t *query_table
);

#endif
