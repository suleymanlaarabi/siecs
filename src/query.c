#include "command_buffer.h"
#include "module.h"
#include "storage/query_index.h"
#include "table.h"
#include "utils.h"
#include "world_internal.h"
#include <stdint.h>
#include <stdlib.h>

static void ecs_query_index_remove_active_id(ecs_query_index_t *index, ecs_query_id_t qid) {
    ecs_query_cache_t *cache = sicore_vec_get_mut(&index->queries, qid, ecs_query_cache_t);
    uint32_t active_index = cache->active_index;
    uint32_t last_index = index->active_ids.size - 1;

    if (active_index != last_index) {
        ecs_query_id_t moved = *sicore_vec_get(&index->active_ids, last_index, ecs_query_id_t);
        ((ecs_query_id_t *)index->active_ids.data)[active_index] = moved;
        sicore_vec_get_mut(&index->queries, moved, ecs_query_cache_t)->active_index = active_index;
    }

    sicore_vec_remove_last(&index->active_ids);
}

ecs_query_id_t ecs_query_init(const ecs_query_desc_t *desc) {
    ecs_assert_not_scheduler_parallel("query registration");
    return ecs_query_index_create(desc);
}

ecs_iter_t ecs_query_iter(ecs_query_id_t query_id) {
    ecs_assert(query_id < query_index.queries.size, "invalid query id: %u\n", query_id);

    ecs_query_cache_t *cache =
        sicore_vec_get_mut(&query_index.queries, query_id, ecs_query_cache_t);
    ecs_assert(cache->alive, "query id is not alive: %u\n", query_id);
    return (ecs_iter_t){
        .cache = cache,
        .table_idx = UINT16_MAX,
        .table_count = cache->table_count,
        .count = 0,
    };
}

uint32_t ecs_query_count(ecs_query_id_t query_id) {
    ecs_assert(query_id < query_index.queries.size, "invalid query id: %u\n", query_id);
    ecs_query_cache_t *cache =
        sicore_vec_get_mut(&query_index.queries, query_id, ecs_query_cache_t);
    ecs_assert(cache->alive, "query id is not alive: %u\n", query_id);

    uint32_t count = 0;
    for (uint16_t i = 0; i < cache->table_count; i++) {
        const uint16_t table_id = ecs_query_table_id(cache, i);
        const ecs_table_t *table = ecs_get_table(table_id);
        if (ECS_UNLIKELY(cache->query->up_mask) &&
            !ecs_query_resolve_up_fields(cache, table, ecs_query_table_at(cache, i))) {
            continue;
        }
        count += table->entity_count;
    }
    return count;
}

bool ecs_iter_next(ecs_iter_t *it) {
    ecs_query_cache_t *cache = it->cache;
    ecs_table_t *table;
    do {
        if (++it->table_idx >= it->table_count)
            return false;
        const uint16_t table_id = ecs_query_table_id(cache, it->table_idx);
        table = &table_index.tables[table_id];
        it->count = table->entity_count;
        if (it->count && ECS_UNLIKELY(cache->query->up_mask) &&
            !ecs_query_resolve_up_fields(
                cache,
                table,
                ecs_query_table_at(cache, it->table_idx)
            )) {
            it->count = 0;
        }
    } while (it->count == 0);
    if (cache->query->field_count == 0) {
        it->ptrs = NULL;
        it->field_kind_bits = 0;
    } else {
        ecs_query_table_t *query_table = ecs_query_table_at(cache, it->table_idx);
        it->ptrs = query_table->fields;
        it->field_kind_bits = query_table->field_kind_bits;
    }
    it->entities = table->entities;
    return true;
}

const ecs_relation_target_t *ecs_targets_id(const ecs_iter_t *it, ecs_relation_id_t relation) {
    const ecs_relation_record_t *record = ecs_relation_record(relation);
    ecs_assert(record->info.desc.storage != EcsRelationByTarget, "ecs_targets requires Dense or ByDepth\n");
    const uint16_t table_id = ecs_query_table_id(it->cache, it->table_idx);
    const ecs_table_t *table = ecs_get_table(table_id);
    uint16_t column = ecs_table_column_or_invalid(table, record->component);
    return column == UINT16_MAX ? NULL : table->cls[column].data;
}

ecs_entity_t ecs_target_shared_id(const ecs_iter_t *it, ecs_relation_id_t relation) {
#ifndef NDEBUG
    const ecs_relation_record_t *record = ecs_relation_record(relation);
    ecs_assert(record->info.desc.storage == EcsRelationByTarget, "ecs_target_shared requires ByTarget\n");

#endif
    const uint16_t table_id = ecs_query_table_id(it->cache, it->table_idx);
    const ecs_table_t *table = ecs_get_table(table_id);
    return ecs_table_target_id(table, relation);
}

void ecs_query_fini(ecs_query_id_t qid) {
    ecs_assert(qid < query_index.queries.size, "invalid query id: %u\n", qid);

    ecs_query_cache_t *cache =
        sicore_vec_get_mut(&query_index.queries, qid, ecs_query_cache_t);
    ecs_assert(cache->alive, "query id is not alive: %u\n", qid);

    if (cache->active_index != UINT32_MAX) {
        ecs_query_index_remove_active_id(&query_index, qid);
    }
    free(cache->query);
    free(cache->tables);
    cache->query = NULL;
    cache->tables = NULL;
    cache->table_capacity = 0;
    cache->table_count = 0;
    cache->next_free = query_index.first_free;
    cache->alive = false;
    query_index.first_free = qid;
}
