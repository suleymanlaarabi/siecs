#include "datastructure/vec.h"
#include "storage/query_index.h"
#include "table.h"
#include "utils.h"
#include "world_internal.h"
#include <stdint.h>
#include <stdlib.h>

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

ecs_query_id_t ecs_query_init(const ecs_query_desc_t *desc) {
        ecs_query_id_t qid = ecs_query_index_create(&ecs_world.query_index, desc);
    ecs_query_index_update_matches(
                ecs_vec_get_mut(&ecs_world.query_index.queries, qid, ecs_query_cache_t)
    );
    return qid;
}

ecs_iter_t ecs_query_iter(ecs_query_id_t query_id) {
        ecs_assert(query_id < ecs_world.query_index.queries.size, "invalid query id: %u\n", query_id);

    ecs_query_cache_t *cache =
        ecs_vec_get_mut(&ecs_world.query_index.queries, query_id, ecs_query_cache_t);
    ecs_assert(cache->alive, "query id is not alive: %u\n", query_id);
    return (ecs_iter_t){
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
        it->count = ecs_world.table_index.tables[tids[it->table_idx]].entity_count;
    } while (it->count == 0);
    if (it->cache->query.field_count == 0) {
        it->ptrs = NULL;
        it->field_kinds = NULL;
    } else {
        it->ptrs = &it->cache->fields_ptr[it->table_idx * it->cache->query.field_count];
        it->field_kinds = &it->cache->fields_kind[it->table_idx * it->cache->query.field_count];
    }
    it->entities = ecs_world.table_index.tables[tids[it->table_idx]].entities;
    return true;
}
void ecs_query_fini(ecs_query_id_t qid) {
        ecs_assert(qid < ecs_world.query_index.queries.size, "invalid query id: %u\n", qid);

    ecs_query_cache_t *cache = ecs_vec_get_mut(&ecs_world.query_index.queries, qid, ecs_query_cache_t);
    ecs_assert(cache->alive, "query id is not alive: %u\n", qid);

    ecs_query_index_destroy(&cache->query);
    free(cache->fields_ptr);
    free(cache->fields_kind);
    ecs_vec_fini(&cache->table_ids);
    cache->fields_ptr = NULL;
    cache->fields_kind = NULL;
    cache->field_table_capacity = 0;

    ecs_query_index_remove_active_id(&ecs_world.query_index, qid);
    cache->next_free = ecs_world.query_index.first_free;
    cache->alive = false;
    ecs_world.query_index.first_free = qid;
}
