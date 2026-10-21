#include "storage/query_index.h"
#include "table_ops.h"
#include "world_internal.h"
#include <stdlib.h>
#include <string.h>
ecs_query_id_t ecs_query_init(const ecs_query_desc_t *desc) {
    ecs_assert_not_scheduler_parallel("query registration");
    return ecs_query_index_create(desc);
}

ecs_iter_t ecs_query_iter_at(ecs_query_id_t query_id, uint64_t tick, bool commit) {
    ecs_query_cache_t *cache = ecs_query_cache(query_id);
    return (ecs_iter_t){
        .cache = cache,
        .table_idx = UINT16_MAX,
        .table_count = cache->table_count,
        .count = 0,
        .commit = commit,
        .tick = tick,
        .last_run = cache->last_run,
    };
}
ecs_iter_t ecs_query_iter(ecs_query_id_t query_id) {
    return ecs_query_iter_at(query_id, ecs_change_tick_take(), true);
}
uint32_t ecs_query_count(ecs_query_id_t query_id) {
    // Counts every match: no change filter, no write stamps, no recorded run.
    ecs_iter_t it = ecs_query_iter_at(query_id, 0, false);
    it.last_run = 0;
    uint32_t count = 0;
    while (ecs_iter_next(&it))
        count += it.count;
    return count;
}
static bool ecs_query_field_changed(
    const ecs_query_t *query,
    const ecs_table_t *table,
    uint16_t field,
    uint32_t field_kind_bits,
    uint64_t last_run
) {
    ecs_field_kind_t kind = (ecs_field_kind_t)((field_kind_bits >> (2 * field)) & 3);
    if (kind == EcsFieldNone)
        return false;
    ecs_component_location_t source =
        ecs_component_locate(table, 0, ecs_query_fields(query)[field].id, false);
    if (!source.table)
        return false;
    // A shared value also changes when its base moves to another table.
    return ecs_table_column_tick(source.table, source.column) > last_run ||
           (source.shared && source.table->structure_tick > last_run);
}

bool ecs_query_table_changed(
    const ecs_query_t *query,
    const ecs_table_t *table,
    uint32_t field_kind_bits,
    uint64_t last_run
) {
    if (table->structure_tick > last_run)
        return true;
    for (uint16_t mask = query->changed_mask; mask; mask &= (uint16_t)(mask - 1)) {
        if (ecs_query_field_changed(query, table, ECS_CTZ(mask), field_kind_bits, last_run))
            return true;
    }
    return false;
}

static void ecs_query_stamp_writes(
    const ecs_query_t *query,
    ecs_table_t *table,
    uint32_t field_kind_bits,
    uint64_t tick
) {
    for (uint16_t mask = query->write_mask; mask; mask &= (uint16_t)(mask - 1)) {
        uint16_t field = (uint16_t)ECS_CTZ(mask);
        if (((field_kind_bits >> (2 * field)) & 3) != EcsFieldOwned)
            continue;
        uint16_t column = ecs_table_column_or_invalid(table, ecs_query_fields(query)[field].id);
        if (column != UINT16_MAX)
            ecs_table_column_changed(table, column, tick);
    }
}

bool ecs_iter_next(ecs_iter_t *it) {
    ecs_query_cache_t *cache = it->cache;
    const ecs_query_t *query = cache->query;
    while (++it->table_idx < it->table_count) {
        ecs_table_t *table = ecs_get_table(ecs_query_table_id(cache, it->table_idx));
        if (table->entity_count <= it->row_offset)
            continue;
        it->count = table->entity_count - it->row_offset;
        if (it->bounded && it->count > it->row_limit)
            it->count = it->row_limit;
        if (!it->count)
            continue;
        it->field_kind_bits = 0;
        it->ptrs = NULL;
        if (query->field_count) {
            ecs_query_table_t *entry = ecs_query_table_at(cache, it->table_idx);
            it->field_kind_bits = entry->field_kind_bits;
            it->ptrs = entry->fields;
            if (query->up_mask || it->bounded) {
                memcpy(it->field_storage, entry->fields, query->field_count * sizeof(void *));
                it->ptrs = it->field_storage;
                if (query->up_mask &&
                    !ecs_query_resolve_up_fields(query, table, it->ptrs, &it->field_kind_bits))
                    continue;
                for (uint8_t i = 0; it->row_offset && i < query->field_count; i++) {
                    if (((it->field_kind_bits >> (2 * i)) & 3) == EcsFieldOwned) {
                        size_t size =
                            ecs_component_index_get(ecs_query_fields(query)[i].id)->info->size;
                        if (size)
                            it->ptrs[i] = (uint8_t *)it->ptrs[i] + size * it->row_offset;
                    }
                }
            }
        }
        if (query->changed_mask &&
            !ecs_query_table_changed(query, table, it->field_kind_bits, it->last_run))
            continue;
        if (query->write_mask && it->tick)
            ecs_query_stamp_writes(query, table, it->field_kind_bits, it->tick);
        it->entities = table->entities + it->row_offset;
        return true;
    }
    if (it->commit)
        cache->last_run = it->tick;
    return false;
}
uint16_t ecs_iter_table_id(const ecs_iter_t *it) {
    return ecs_query_table_id(it->cache, it->table_idx);
}
bool ecs_iter_changed(const ecs_iter_t *it, uint16_t field_index) {
    const ecs_table_t *table = ecs_get_table(ecs_iter_table_id(it));
    if (table->structure_tick > it->last_run)
        return true;
    const ecs_query_t *query = it->cache->query;
    return ecs_query_field_changed(query, table, field_index, it->field_kind_bits, it->last_run);
}
bool ecs_iter_structure_changed(const ecs_iter_t *it) {
    return ecs_get_table(ecs_iter_table_id(it))->structure_tick > it->last_run;
}
const ecs_relation_target_t *ecs_targets_id(const ecs_iter_t *it, ecs_relation_id_t relation) {
    const ecs_relation_record_t *record = ecs_relation_record(relation);
    ecs_assert(
        record->info.desc.storage != EcsRelationByTarget,
        "ecs_targets requires Dense or ByDepth\n"
    );
    const ecs_table_t *table = ecs_get_table(ecs_iter_table_id(it));
    uint16_t column = ecs_table_column_or_invalid(table, record->component);
    return column == UINT16_MAX
               ? NULL
               : (const ecs_relation_target_t *)table->cls[column].data + it->row_offset;
}
ecs_entity_t ecs_target_shared_id(const ecs_iter_t *it, ecs_relation_id_t relation) {
    const ecs_table_t *table = ecs_get_table(ecs_iter_table_id(it));
    return ecs_table_target_id(table, relation);
}

ecs_relation_batch_t ecs_relation_batch_id(const ecs_iter_t *it, ecs_relation_id_t relation) {
    const ecs_relation_record_t *record = ecs_relation_record(relation);
    const ecs_table_t *table = ecs_get_table(ecs_iter_table_id(it));
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
    ecs_assert_not_scheduler_parallel("query destruction");
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
