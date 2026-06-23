#include "datastructure/vec.h"
#include "storage/query_index.h"
#include "table.h"
#include "utils.h"
#include "world_internal.h"
#include <stdint.h>

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
    ecs_assert(
        query_id < world->query_index.queries.size,
        "invalid query id: %u\n",
        query_id
    );

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
    } else {
        void ***fields = it->cache->fields.data;
        it->ptrs = &fields[it->table_idx * it->cache->query.field_count];
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
    ecs_vec_fini(&cache->fields);
    ecs_vec_fini(&cache->table_ids);

    ecs_query_index_remove_active_id(&world->query_index, qid);
    cache->next_free = world->query_index.first_free;
    cache->alive = false;
    world->query_index.first_free = qid;
}
