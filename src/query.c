#include "datastructure/vec.h"
#include "storage/query_index.h"
#include "table.h"
#include "utils.h"
#include "world_internal.h"
#include <stdint.h>

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

    ecs_query_cache_t *cache =
        ecs_vec_get_mut(&world->query_index.queries, query_id, ecs_query_cache_t);
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
    return true;
}

ecs_table_t *ecs_iter_table(ecs_iter_t *it) {
    uint16_t tid = *ecs_vec_get_mut(&it->cache->table_ids, it->table_idx, uint16_t);
    return ecs_table_index_at(&it->world->table_index, tid);
}

void ecs_query_fini(ecs_world_t *world, ecs_query_id_t qid) {
    ecs_query_cache_t *cache = ecs_vec_get_mut(&world->query_index.queries, qid, ecs_query_cache_t);

    ecs_query_index_destroy(&cache->query);
    ecs_vec_fini(&cache->fields);
    ecs_vec_fini(&cache->table_ids);

    ecs_vec_remove_fast(&world->query_index.queries, qid, sizeof(ecs_query_cache_t));
}
