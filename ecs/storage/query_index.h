#pragma once
#include "ecs/datastructure/vec.h"
#include "ecs/table.h"
#include "ecs/world.h"
#include <stdint.h>

typedef struct {
    uint64_t bloom;
    ecs_component_t *required;
    ecs_component_t *excluded;
    uint16_t required_count;
    uint16_t excluded_count;
} ecs_query_t;

typedef struct {
    ecs_query_t query;
    ecs_vec_t matches; // uint16_t table ids
} ecs_query_cache_t;

typedef struct {
    ecs_vec_t queries;
} ecs_query_index_t;

void ecs_query_index_init(ecs_query_index_t *index);
void ecs_query_index_fini(ecs_query_index_t *index);
uint16_t ecs_query_index_create(ecs_query_index_t *index, const ecs_query_desc_t *desc);
void ecs_query_index_update_matches(
    ecs_query_index_t *index,
    const ecs_table_t *tables,
    uint16_t table_count,
    uint16_t query
);
void ecs_query_index_add_table(
    ecs_query_index_t *index,
    const ecs_table_t *table,
    uint16_t table_id
);

// Reusable query helpers shared with the observer index.
void ecs_query_from_desc(const ecs_query_desc_t *desc, ecs_query_t *query);
void ecs_query_fini(ecs_query_t *query);

static inline bool ecs_query_match_table(const ecs_query_t *query, const ecs_table_t *table) {
    if ((query->bloom & table->bloom) != query->bloom) {
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
    return true;
}
