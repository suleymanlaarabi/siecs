#ifndef SIECS_STORAGE_QUERY_INDEX_H
#define SIECS_STORAGE_QUERY_INDEX_H
#include "../helper.h"
#include "../table.h"
#include "../utils.h"
#include "siecs.h"

typedef struct {
    uint64_t bloom;
    ecs_entity_t is_a;
    ecs_query_order_t order_by;
    uint16_t up_mask, stride;
    uint8_t field_count, match_count, resource_count, filter_count;
} ecs_query_t;

static inline size_t ecs_query_size(const ecs_query_t *q) {
    return sizeof(*q) + (q->field_count + q->match_count + q->resource_count) *
        sizeof(ecs_access_term_t) + q->filter_count * sizeof(ecs_query_relation_term_t);
}

static inline const ecs_component_term_t *ecs_query_fields(const ecs_query_t *q) {
    return (const ecs_component_term_t *)(q + 1);
}
static inline const ecs_component_term_t *ecs_query_match_terms(const ecs_query_t *q) {
    return ecs_query_fields(q) + q->field_count;
}
static inline const ecs_resource_term_t *ecs_query_resources(const ecs_query_t *q) {
    return ecs_query_match_terms(q) + q->match_count;
}
static inline const ecs_query_relation_term_t *ecs_query_filters(const ecs_query_t *q) {
    return (const ecs_query_relation_term_t *)(ecs_query_resources(q) + q->resource_count);
}
static inline ecs_access_t ecs_access_term_access(ecs_access_term_t term) {
    return (ecs_access_t)(term.access & UINT32_C(0xff));
}
static inline ecs_relation_id_t ecs_access_term_source_relation(ecs_access_term_t term) {
    return (ecs_relation_id_t)(term.access >> 8);
}

typedef struct {
    uint16_t id, _padding;
    uint32_t field_kind_bits;
    void *fields[];
} ecs_query_table_t;

typedef struct ecs_query_cache_s {
    ecs_query_t *query;
    uint8_t *tables;
    ecs_id_map_t positions;
    uint32_t active_index, table_capacity;
    ecs_observer_id_t observer;
    uint16_t table_count, next_free;
    bool alive;
} ecs_query_cache_t;

_Static_assert(sizeof(ecs_query_cache_t) <= 64, "query cache must fit a cache line");

static inline ecs_query_table_t *ecs_query_table_at(const ecs_query_cache_t *cache, uint16_t i) {
    return (ecs_query_table_t *)(cache->tables + (size_t)i * cache->query->stride);
}
static inline uint8_t *ecs_query_table_bytes_at(const ecs_query_cache_t *cache, uint16_t i) {
    return cache->tables + (size_t)i * cache->query->stride;
}
static inline uint16_t ecs_query_table_id(const ecs_query_cache_t *cache, uint16_t i) {
    if (!cache->query->field_count) return ((const uint16_t *)cache->tables)[i];
    return *(const uint16_t *)(cache->tables + (size_t)i * cache->query->stride);
}
static inline uint16_t ecs_query_table_position(const ecs_query_cache_t *cache, uint16_t id) {
    if (cache->positions.ids) return ecs_id_map_at_or_invalid(&cache->positions, id);
    for (uint16_t i = 0; i < cache->table_count; i++)
        if (ecs_query_table_id(cache, i) == id) return i;
    return UINT16_MAX;
}

typedef struct {
    sicore_vec_t queries, active_ids;
    uint16_t first_free;
} ecs_query_index_t;

extern ecs_query_index_t query_index;

static inline ecs_query_cache_t *ecs_query_cache(ecs_query_id_t id) {
    ecs_assert(id < query_index.queries.size, "invalid query id: %u\n", id);
    ecs_query_cache_t *cache = sicore_vec_get_mut(&query_index.queries, id, ecs_query_cache_t);
    ecs_assert(cache->alive, "query id is not alive: %u\n", id);
    return cache;
}

void ecs_query_index_init(void);
void ecs_query_index_fini(void);
ecs_query_id_t ecs_query_index_create(const ecs_query_desc_t *desc);
void ecs_query_index_activate(ecs_query_id_t id, const uint16_t *tables, uint16_t count);
void ecs_query_index_add_table(const ecs_table_t *table, uint16_t table_id);
void ecs_query_index_refresh_table_fields(const ecs_table_t *table, uint16_t table_id);
bool ecs_query_resolve_up_fields(ecs_query_cache_t *cache, const ecs_table_t *table,
                                 ecs_query_table_t *entry);
#endif
