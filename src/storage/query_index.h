#ifndef SIECS_STORAGE_QUERY_INDEX_H
#define SIECS_STORAGE_QUERY_INDEX_H
#include "../compiler.h"
#include "../datastructure/vec.h"
#include "../table.h"
#include "siecs.h"
#include <stdint.h>

typedef struct {
    uint64_t bloom;
    ecs_query_term_t *terms;
    ecs_query_term_t *fields;
    uint16_t term_count;
    uint16_t field_count;
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
    for (uint16_t i = 0; i < query->term_count; i++) {
        ecs_query_term_t term = query->terms[i];
        if (term.access == EcsInOptional || term.access == EcsInOutOptional) {
            continue;
        } else if (term.access == EcsNot) {
            if (ecs_table_has(table, term.id)) {
                return false;
            }
        } else if (!ecs_table_has(table, term.id)) {
            return false;
        }
    }
    return true;
}

#endif
