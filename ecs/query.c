#include "ecs/utils.h"
#include "ecs/world.h"
#include "ecs/world_internal.h"

uint32_t ecs_query_init(ecs_world_t *world, const ecs_query_desc_t *desc) {
    ecs_assert_not_null(world);
    uint32_t qid = ecs_query_index_create(&world->query_index, desc);
    ecs_query_index_update_matches(
        &world->query_index,
        world->table_index.tables,
        world->table_index.table_count,
        qid
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
    it->table_idx++;
    if (it->table_idx >= it->table_count) {
        return false;
    }
    it->ptrs = &((void ***)it->cache->fields.data)[it->table_idx * it->cache->query.read_count];
    it->count =
        it->world->table_index.tables[((uint16_t *)it->cache->table_ids.data)[it->table_idx]]
            .entity_count;
    return true;
}

ecs_table_t *ecs_iter_table(ecs_iter_t *it) {
    uint16_t tid = *ecs_vec_get_mut(&it->cache->table_ids, it->table_idx, uint16_t);
    return ecs_table_index_at(&it->world->table_index, tid);
}
