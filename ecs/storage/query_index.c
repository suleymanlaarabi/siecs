#include "query_index.h"
#include "ecs/datastructure/vec.h"
#include "ecs/table.h"
#include "ecs/type.h"
#include "ecs/world.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void ecs_query_index_init(ecs_query_index_t *index) {
    ecs_vec_init(&index->queries, sizeof(ecs_query_cache_t));
}

void ecs_query_index_fini(ecs_query_index_t *index) {
    for (uint32_t i = 0; i < index->queries.size; i++) {
        ecs_query_cache_t *cache = ecs_vec_get_mut(&index->queries, i, ecs_query_cache_t);
        ecs_vec_fini(&cache->matches);
        ecs_query_fini(&cache->query);
    }
    ecs_vec_fini(&index->queries);
}

void ecs_query_fini(ecs_query_t *query) {
    free(query->required);
    free(query->excluded);
}

void ecs_query_from_desc(const ecs_query_desc_t *desc, ecs_query_t *query) {
    int i = 0;
    for (; desc->required[i]; i++) {
    }
    query->required = malloc(sizeof(ecs_component_t) * i);
    memcpy(query->required, desc->required, sizeof(ecs_component_t) * i);
    query->required_count = i;
    for (i = 0; desc->excluded[i]; i++)
        ;
    query->excluded = malloc(sizeof(ecs_component_t) * i);
    query->excluded_count = i;
    memcpy(query->excluded, desc->excluded, sizeof(ecs_component_t) * i);
    ecs_type_t type = { .ids = query->required, .count = query->required_count };
    query->bloom = ecs_type_bloom(&type);
}

uint32_t ecs_query_index_create(ecs_query_index_t *index, const ecs_query_desc_t *desc) {
    ecs_query_cache_t *query_cache = ecs_vec_push_empty(&index->queries, sizeof(ecs_query_cache_t));
    ecs_query_from_desc(desc, &query_cache->query);
    ecs_vec_init(&query_cache->matches, sizeof(uint16_t));

    return index->queries.size - 1;
}

void ecs_query_index_update_matches(
    ecs_query_index_t *index,
    const ecs_table_t *tables,
    uint16_t table_count,
    uint32_t query
) {
    ecs_query_cache_t *query_cache = ecs_vec_get_mut(&index->queries, query, ecs_query_cache_t);

    for (uint16_t i = 0; i < table_count; i++) {
        if (ecs_query_match_table(&query_cache->query, &tables[i])) {
            ecs_vec_push_u16(&query_cache->matches, i);
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
            ecs_vec_push_u16(&cache->matches, table_id);
        }
    }
}
