#include "storage/query_index.h"
#include "world_internal.h"
#include <stdlib.h>
ecs_query_id_t ecs_query_init(const ecs_query_desc_t *desc) {
    ecs_assert_not_scheduler_parallel("query registration");
    return ecs_query_index_create(desc);
}

ecs_iter_t ecs_query_iter(ecs_query_id_t query_id) {
    ecs_query_cache_t *cache = ecs_query_cache(query_id);
    return (ecs_iter_t){
        .cache = cache,
        .table_idx = UINT16_MAX,
        .table_count = cache->table_count,
        .count = 0,
    };
}
ecs_iter_t
ecs_query_iter_target(ecs_query_id_t query_id, ecs_relation_id_t relation, ecs_entity_t target) {
    ecs_assert(
        ecs_relation_record(relation)->info.desc.storage == EcsRelationByTarget,
        "scoped iteration requires ByTarget"
    );
    ecs_iter_t it = ecs_query_iter(query_id);
    it.scope_relation = relation;
    it.scope_target = target;
    return it;
}

uint32_t ecs_query_count(ecs_query_id_t query_id) {
    ecs_iter_t it = ecs_query_iter(query_id);
    uint32_t count = 0;
    while (ecs_iter_next(&it))
        count += it.count;
    return count;
}

bool ecs_iter_next(ecs_iter_t *it) {
    ecs_query_cache_t *cache = it->cache;
    const ecs_query_t *q = cache->query;
    ecs_table_t *table = NULL;
    if (it->table_idx != UINT16_MAX && it->row_cursor) {
        table = ecs_get_table(ecs_query_table_id(cache, it->table_idx));
        if (it->row_cursor >= table->entity_count)
            table = NULL;
    }
    for (;;) {
        if (!table) {
            if (++it->table_idx >= it->table_count)
                return false;
            table = ecs_get_table(ecs_query_table_id(cache, it->table_idx));
            it->row_cursor = 0;
            if (!table->entity_count ||
                (it->scope_relation &&
                 ecs_table_target_id(table, it->scope_relation) != it->scope_target) ||
                ((q->source_flags & 1) && !ecs_query_source_matches(q, table, 0, false)) ||
                (q->up_mask && !ecs_query_resolve_up_fields(
                                   cache,
                                   table,
                                   ecs_query_table_at(cache, it->table_idx)
                               ))) {
                table = NULL;
                continue;
            }
        }
        uint32_t first = it->row_cursor;
        uint32_t end = table->entity_count;
        if ((q->source_flags & 2)) {
            while (first < end && !ecs_query_source_matches(q, table, first, true))
                first++;
            if (first == end) {
                table = NULL;
                continue;
            }
            end = first + 1;
            while (end < table->entity_count && ecs_query_source_matches(q, table, end, true))
                end++;
        }
        it->row_cursor = end;
        it->row_offset = first;
        it->count = end - first;
        it->entities = table->entities + first;
        if (!q->field_count) {
            it->ptrs = NULL;
            it->field_kind_bits = 0;
        } else {
            ecs_query_table_t *entry = ecs_query_table_at(cache, it->table_idx);
            it->field_kind_bits = entry->field_kind_bits;
            it->ptrs = entry->fields;
            if (first) {
                for (uint8_t i = 0; i < q->field_count; i++) {
                    it->field_views[i] = entry->fields[i];
                    if (((entry->field_kind_bits >> (i * 2)) & 3) == EcsFieldOwned)
                        it->field_views[i] =
                            (uint8_t *)entry->fields[i] +
                            first * ecs_component_info(ecs_query_fields(q)[i].id)->size;
                }
                it->ptrs = it->field_views;
            }
        }
        return true;
    }
}
const ecs_relation_target_t *ecs_targets_id(const ecs_iter_t *it, ecs_relation_id_t relation) {
    const ecs_relation_record_t *record = ecs_relation_record(relation);
    ecs_assert(
        record->info.desc.storage != EcsRelationByTarget,
        "ecs_targets requires Dense or ByDepth\n"
    );
    const uint16_t table_id = ecs_query_table_id(it->cache, it->table_idx);
    const ecs_table_t *table = ecs_get_table(table_id);
    uint16_t column = ecs_table_column_or_invalid(table, record->component);
    return column == UINT16_MAX
               ? NULL
               : (const ecs_relation_target_t *)table->cls[column].data + it->row_offset;
}
ecs_entity_t ecs_target_shared_id(const ecs_iter_t *it, ecs_relation_id_t relation) {
    const uint16_t table_id = ecs_query_table_id(it->cache, it->table_idx);
    const ecs_table_t *table = ecs_get_table(table_id);
    return ecs_table_target_id(table, relation);
}

ecs_relation_batch_t ecs_relation_batch_id(const ecs_iter_t *it, ecs_relation_id_t relation) {
    const ecs_relation_record_t *record = ecs_relation_record(relation);
    const uint16_t table_id = ecs_query_table_id(it->cache, it->table_idx);
    const ecs_table_t *table = ecs_get_table(table_id);
    if (record->component == 0 || record->info.desc.storage == EcsRelationByTarget) {
        return (ecs_relation_batch_t){
            .shared_target = ecs_type_pair_get(&table->type, relation),
            .shared = true,
        };
    }
    const uint16_t column = ecs_table_column_or_invalid(table, record->component);
    return (ecs_relation_batch_t){
        .targets = column == UINT16_MAX
                       ? NULL
                       : (const ecs_relation_target_t *)table->cls[column].data + it->row_offset,
        .shared = false,
    };
}

void ecs_query_fini(ecs_query_id_t qid) {
    ecs_query_cache_t *cache = ecs_query_cache(qid);
    if (cache->active_index != UINT32_MAX) {
        ecs_query_id_t *ids = query_index.active_ids.data;
        ecs_query_id_t moved = ids[query_index.active_ids.size - 1];
        ids[cache->active_index] = moved;
        ecs_query_cache(moved)->active_index = cache->active_index;
        sicore_vec_remove_last(&query_index.active_ids);
    }
    free(cache->query);
    free(cache->tables);
    cache->alive = false;
    cache->next_free = query_index.first_free;
    query_index.first_free = qid;
}
