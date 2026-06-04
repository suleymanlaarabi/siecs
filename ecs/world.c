#include "ecs/world.h"
#include "./id.h"
#include "ecs/datastructure/bitset.h"
#include "ecs/datastructure/idmap.h"
#include "ecs/datastructure/vec.h"
#include "ecs/storage/entity_index.h"
#include "ecs/storage/query_index.h"
#include "ecs/storage/table_index.h"
#include "ecs/table.h"
#include "ecs/type.h"
#include "ecs/utils.h"
#include "world_internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define ecs_get_record(world, entity)                                                              \
    ecs_vec_get_mut(&world->entity_index.entities, ecs_first(entity), ecs_entity_record_t)
#define ecs_get_table(world, tid) ecs_table_index_at(&world->table_index, tid)

ecs_world_t *ecs_init() {
    ecs_world_t *world = malloc(sizeof(ecs_world_t));
    ecs_entity_index_init(&world->entity_index);
    ecs_component_index_init(&world->component_index);
    ecs_table_index_init(&world->table_index);
    ecs_query_index_init(&world->query_index);
    ecs_observer_index_init(&world->observer_index);

    // Create empty table (index 0)
    ecs_table_index_get_or_create(&world->table_index, (ecs_type_t){ 0 }, &world->component_index);

    // Reserve entity id 0
    ecs_new(world);

    // Reserve component id 0
    ecs_component(world, {});

    return world;
}

ecs_entity_t ecs_new(ecs_world_t *world) {
    ecs_assert_not_null(world);
    ecs_table_t *table = ecs_get_table(world, 0);

    ecs_entity_t entity = ecs_entity_index_create(&world->entity_index, table->entity_count);
    ecs_table_add_entity(table, entity);

    return entity;
}

ecs_component_t ecs_component_init(ecs_world_t *world, const ecs_component_desc_t *desc) {
    ecs_assert_not_null(world);
    return ecs_component_index_create(
        &world->component_index,
        desc->name,
        desc->size,
        desc->is_bitset
    );
}

static inline void
copy_column(ecs_column_t *from, uint32_t from_row, ecs_column_t *to, uint32_t to_row) {
    if (from->is_bitset) {
        ecs_bit_set src_bs = { .words = (uint64_t *)from->data };
        ecs_bit_set dst_bs = { .words = (uint64_t *)to->data };
        if (ecs_bitset_is_set(&src_bs, from_row)) {
            ecs_bitset_set(&dst_bs, to_row);
        } else {
            ecs_bitset_unset(&dst_bs, to_row);
        }
    } else if (from->size > 0) {
        memcpy(
            (uint8_t *)to->data + (from->size * to_row),
            (uint8_t *)from->data + (from->size * from_row),
            from->size
        );
    }
}

static inline void migrate_entity_add(
    ecs_world_t *world,
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_id,
    ecs_component_t added_id
) {
    ecs_table_t *to_table = ecs_get_table(world, to_id);

    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    uint16_t k = ecs_table_get_column_index(to_table, added_id);
    for (uint16_t i = 0; i < k; i++)
        copy_column(&from_table->cls[i], old_row, &to_table->cls[i], new_row);
    for (uint16_t i = k + 1; i < to_table->type.count; i++)
        copy_column(&from_table->cls[i - 1], old_row, &to_table->cls[i], new_row);

    ecs_entity_t moved = ecs_table_remove_entity(from_table, old_row);
    if (moved != entity)
        ecs_get_record(world, moved)->table_row = old_row;

    record->table_id = to_id;
    record->table_row = new_row;
}

static inline void migrate_entity_remove(
    ecs_world_t *world,
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_id,
    uint16_t col_idx
) {
    ecs_table_t *to_table = ecs_get_table(world, to_id);

    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    for (uint16_t i = 0; i < col_idx; i++)
        copy_column(&from_table->cls[i], old_row, &to_table->cls[i], new_row);
    for (uint16_t i = col_idx + 1; i < from_table->type.count; i++)
        copy_column(&from_table->cls[i], old_row, &to_table->cls[i - 1], new_row);

    ecs_entity_t moved = ecs_table_remove_entity(from_table, old_row);
    if (moved != entity)
        ecs_get_record(world, moved)->table_row = old_row;

    record->table_id = to_id;
    record->table_row = new_row;
}

static inline void ecs_emit(
    ecs_world_t *world,
    ecs_table_t *table,
    ecs_entity_t entity,
    ecs_event_t event,
    void *data
) {
    if (event >= table->observers_by_event.size) {
        return;
    }
    // Snapshot the count: observers registered by a callback must not fire for
    // the event already in flight. Re-fetch the per-event list each iteration in
    // case a nested registration reallocs the outer vec or the inner list.
    uint32_t n = ecs_vec_get_mut(&table->observers_by_event, event, ecs_vec_t)->size;
    for (uint32_t i = 0; i < n; i++) {
        ecs_vec_t *list = ecs_vec_get_mut(&table->observers_by_event, event, ecs_vec_t);
        uint32_t oid = *ecs_vec_get(list, i, uint32_t);
        ecs_observer_t *obs =
            ecs_vec_get_mut(&world->observer_index.observers, oid, ecs_observer_t);
        obs->callback(world, entity, data);
    }
}

void ecs_add_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    uint16_t from_id = record->table_id;
    ecs_table_t *table = ecs_get_table(world, from_id);

    uint16_t new_table_id = ecs_table_get_add_edge(table, cid);

    if (new_table_id == UINT16_MAX) {
        uint32_t old_count = world->table_index.tables.size;
        ecs_type_t new_type = ecs_type_with_add(&table->type, cid);
        new_table_id =
            ecs_table_index_get_or_create(&world->table_index, new_type, &world->component_index);
        // Re-fetch: ecs_table_index_get_or_create may realloc the tables vec
        table = ecs_get_table(world, from_id);
        ecs_id_map_set(&table->add_edge, cid, new_table_id);
        if ((uint32_t)new_table_id == old_count) {
            ecs_query_index_add_table(
                &world->query_index,
                ecs_get_table(world, new_table_id),
                new_table_id
            );
            ecs_observer_index_add_table(
                &world->observer_index,
                ecs_get_table(world, new_table_id)
            );
        }
    } else if (new_table_id < table->type.count && table->type.ids[new_table_id] == cid) {
        return;
    }

    migrate_entity_add(world, record, entity, table, new_table_id, cid);

    ecs_emit(world, ecs_get_table(world, new_table_id), entity, OnAdd, NULL);
}

void ecs_remove_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    uint16_t from_id = record->table_id;
    ecs_table_t *table = ecs_get_table(world, from_id);

    int col_idx = ecs_table_get_column_index(table, cid);

    if (col_idx == UINT16_MAX || col_idx >= table->type.count || table->type.ids[col_idx] != cid) {
        return;
    }

    uint16_t new_table_id = table->cls[col_idx].remove_edge;
    if (new_table_id == UINT16_MAX) {
        uint32_t old_count = world->table_index.tables.size;
        ecs_type_t new_type = ecs_type_with_remove(&table->type, cid);
        new_table_id =
            ecs_table_index_get_or_create(&world->table_index, new_type, &world->component_index);
        // Re-fetch: ecs_table_index_get_or_create may realloc the tables vec
        table = ecs_get_table(world, from_id);
        table->cls[col_idx].remove_edge = new_table_id;
        if ((uint32_t)new_table_id == old_count) {
            ecs_query_index_add_table(
                &world->query_index,
                ecs_get_table(world, new_table_id),
                new_table_id
            );
            ecs_observer_index_add_table(
                &world->observer_index,
                ecs_get_table(world, new_table_id)
            );
        }
    }

    ecs_emit(world, table, entity, OnRemove, NULL);

    migrate_entity_remove(world, record, entity, table, new_table_id, (uint16_t)col_idx);
}
void *ecs_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);
    return ecs_table_get_component(table, cid, record->table_row);
}

void ecs_kill(ecs_world_t *world, ecs_entity_t entity) {
    ecs_assert_not_null(world);
    ecs_assert_entity_valid(entity);
    ecs_is_alive(world, entity);

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);

    // Remove from table
    ecs_entity_t moved = ecs_table_remove_entity(table, record->table_row);
    if (moved != entity) {
        ecs_get_record(world, moved)->table_row = record->table_row;
    }

    ecs_entity_index_kill(&world->entity_index, ecs_first(entity));
}

bool ecs_has_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id) {
    ecs_assert_not_null(world);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    uint16_t tid = ecs_get_record(world, entity)->table_id;
    return ecs_table_has(ecs_get_table(world, tid), id);
}

int ecs_is_alive(ecs_world_t *world, ecs_entity_t entity) {
    return ecs_entity_index_is_alive(&world->entity_index, entity);
}

void ecs_fini(ecs_world_t *world) {
    ecs_entity_index_fini(&world->entity_index);
    ecs_component_index_fini(&world->component_index);
    ecs_table_index_fini(&world->table_index);
    ecs_query_index_fini(&world->query_index);
    ecs_observer_index_fini(&world->observer_index);
    free(world);
}

void ecs_set_bit(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id, bool value) {
    ecs_assert_not_null(world);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);

    int col_idx = ecs_table_get_column_index(table, id);
    if (col_idx == UINT16_MAX || col_idx >= table->type.count || !table->cls[col_idx].is_bitset) {
        return;
    }

    uint64_t *words = (uint64_t *)table->cls[col_idx].data;
    ecs_bit_set bs = { .words = words };
    if (value)
        ecs_bitset_set(&bs, record->table_row);
    else
        ecs_bitset_unset(&bs, record->table_row);

    ecs_emit(world, table, entity, OnSet, NULL);
}

bool ecs_get_bit(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id) {
    ecs_assert_not_null(world);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);

    int col_idx = ecs_table_get_column_index(table, id);
    if (col_idx == UINT16_MAX || col_idx >= table->type.count || !table->cls[col_idx].is_bitset) {
        return false;
    }

    uint64_t *words = (uint64_t *)table->cls[col_idx].data;
    ecs_bit_set bs = { .words = words };
    return ecs_bitset_is_set(&bs, record->table_row);
}

uint32_t ecs_query_init(ecs_world_t *world, const ecs_query_desc_t *desc) {
    ecs_assert_not_null(world);
    uint32_t qid = ecs_query_index_create(&world->query_index, desc);
    ecs_table_t *tables = ecs_vec_data(&world->table_index.tables, ecs_table_t);
    ecs_query_index_update_matches(
        &world->query_index,
        tables,
        (uint16_t)world->table_index.tables.size,
        qid
    );
    return qid;
}

ecs_event_t ecs_event(ecs_world_t *world) { return world->observer_index.event_count++; }

uint32_t ecs_observer_init(ecs_world_t *world, const ecs_observer_desc_t *desc) {
    ecs_assert_not_null(world);

    uint32_t oid = ecs_observer_index_create(&world->observer_index, desc);
    ecs_table_t *tables = ecs_vec_data(&world->table_index.tables, ecs_table_t);
    ecs_observer_index_match_tables(
        &world->observer_index,
        tables,
        (uint16_t)world->table_index.tables.size,
        oid
    );
    return oid;
}

void ecs_observer_trigger(ecs_world_t *world, ecs_entity_t entity, ecs_event_t event, void *data) {
    ecs_assert_not_null(world);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);
    ecs_emit(world, table, entity, event, data);
}

ecs_iter_t ecs_query_iter(ecs_world_t *world, uint32_t query_id) {
    ecs_assert_not_null(world);

    ecs_query_cache_t *cache =
        ecs_vec_get_mut(&world->query_index.queries, query_id, ecs_query_cache_t);
    return (ecs_iter_t){
        .world = world,
        .query_id = query_id,
        .table_idx = UINT16_MAX,
        .table_count = cache->matches.size,
    };
}

bool ecs_iter_next(ecs_iter_t *it) {
    it->table_idx++;
    return (uint32_t)it->table_idx < it->table_count;
}

ecs_table_t *ecs_iter_table(ecs_iter_t *it) {
    ecs_query_cache_t *cache =
        ecs_vec_get_mut(&it->world->query_index.queries, it->query_id, ecs_query_cache_t);
    uint16_t tid = *ecs_vec_get_mut(&cache->matches, it->table_idx, uint16_t);
    return ecs_table_index_at(&it->world->table_index, tid);
}

void *ecs_field(ecs_iter_t *it, ecs_component_t cid) {
    ecs_assert_id_valid(cid);

    ecs_table_t *table = ecs_iter_table(it);
    uint16_t col = ecs_table_get_column_index(table, cid);
    return table->cls[col].data;
}

ecs_bitfield_t ecs_bitfield(ecs_iter_t *it, ecs_component_t cid) {
    ecs_assert_id_valid(cid);
    ecs_table_t *table = ecs_iter_table(it);
    uint16_t col = ecs_table_get_column_index(table, cid);

    return (ecs_bitfield_t){
        .words = (const uint64_t *)table->cls[col].data,
        .word_count = (table->entity_count + 63u) >> 6,
    };
}
