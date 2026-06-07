#include "query_index.h"
#include "ecs/compiler.h"
#include "ecs/datastructure/vec.h"
#include "ecs/storage/component_index.h"
#include "ecs/table.h"
#include "ecs/utils.h"
#include "ecs/world.h"
#include "ecs/world_internal.h"
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
