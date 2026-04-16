#include "world.h"
#include "./id.h"
#include "./storage/component_index.h"
#include "./storage/entity_index.h"
#include "ecs/datastructure/idmap.h"
#include "ecs/datastructure/vec.h"
#include "ecs/datastructure/bitset.h"
#include "ecs/storage/query_index.h"
#include "ecs/storage/table_index.h"
#include "ecs/table.h"
#include "ecs/type.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct ecs_world_s {
    ecs_entity_index_t entity_index;
    ecs_component_index_t component_index;
    ecs_table_index_t table_index;
    ecs_query_index_t query_index;
};

#define ecs_get_record(world, entity) ecs_vec_get_mut(&world->entity_index.entities, ecs_first(entity), ecs_entity_record_t)
#define ecs_get_table(world, tid) ecs_table_index_at(&world->table_index, tid)

ecs_world_t *ecs_init() {
    ecs_world_t *world = malloc(sizeof(ecs_world_t));
    ecs_entity_index_init(&world->entity_index);
    ecs_component_index_init(&world->component_index);
    ecs_table_index_init(&world->table_index);
    ecs_query_index_init(&world->query_index);

    // Create empty table (index 0)
    ecs_table_index_get_or_create(&world->table_index, (ecs_type_t) {0}, &world->component_index);

    // Reserve entity id 0
    ecs_new(world);

    // Reserve component id 0
    ecs_component(world, {});

    return world;
}

ecs_entity_t ecs_new(ecs_world_t *world) {
    ecs_table_t *table = ecs_get_table(world, 0);

    ecs_entity_t entity = ecs_entity_index_create(&world->entity_index, table->entity_count);
    ecs_table_add_entity(table, entity);

    return entity;
}

ecs_component_t ecs_component_init(ecs_world_t *world, const ecs_component_desc_t *desc) {
    return ecs_component_index_create(&world->component_index, desc->name, desc->size, desc->is_bitset);
}

static inline void migrate_entity(
    ecs_world_t *world,
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    uint16_t from,
    uint16_t to
) {
    ecs_table_t *from_table = ecs_get_table(world, from);
    ecs_table_t *to_table = ecs_get_table(world, to);

    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    // Optimized copy using sorted types (two-pointer approach)
    uint16_t f = 0, t = 0;
    while (f < from_table->type.count && t < to_table->type.count) {
        if (from_table->type.ids[f] == to_table->type.ids[t]) {
            uint64_t size = from_table->cls[f].size;
            if (size > 0) {
                memcpy((uint8_t*)to_table->cls[t].data + (size * new_row),
                       (uint8_t*)from_table->cls[f].data + (size * old_row),
                       size);
            }
            f++; t++;
        } else if (from_table->type.ids[f] < to_table->type.ids[t]) {
            f++;
        } else {
            t++;
        }
    }

    // Remove from old table and update moved entity record if necessary
    ecs_entity_t moved = ecs_table_remove_entity(from_table, old_row);
    if (moved != entity) {
        ecs_get_record(world, moved)->table_row = old_row;
    }

    record->table_id = to;
    record->table_row = new_row;
}

void ecs_add_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id) {
    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);

    uint16_t new_table_id = ecs_table_get_add_edge(table, id);

    if (new_table_id == UINT16_MAX) {
        ecs_type_t new_type = ecs_type_with_add(&table->type, id);
        new_table_id = ecs_table_index_get_or_create(&world->table_index, new_type, &world->component_index);
        ecs_id_map_set(&table->add_edge, id, new_table_id);
    } else if (new_table_id < table->type.count && table->type.ids[new_table_id] == id) {
        // is a column index not need to be added
        return;
    }

    migrate_entity(world, record, entity, record->table_id, new_table_id);
}

void ecs_remove_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id) {
    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);

    int col_idx = ecs_table_get_column_index(table, id);

    if (col_idx == UINT16_MAX || col_idx >= table->type.count || table->type.ids[col_idx] != id) {
        return;
    }

    uint16_t new_table_id = table->cls[col_idx].remove_edge;
    if (new_table_id == UINT16_MAX) {
        ecs_type_t new_type = ecs_type_with_remove(&table->type, id);
        new_table_id = ecs_table_index_get_or_create(&world->table_index, new_type, &world->component_index);
        table->cls[col_idx].remove_edge = new_table_id;
    }

    migrate_entity(world, record, entity, record->table_id, new_table_id);
}
void *ecs_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id) {
    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);
    return ecs_table_get_component(table, id, record->table_row);
}

void ecs_kill(ecs_world_t *world, ecs_entity_t entity) {
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
    free(world);
}

void ecs_set_bit(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id, bool value) {
    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);

    int col_idx = ecs_table_get_column_index(table, id);
    if (col_idx == UINT16_MAX || col_idx >= table->type.count || !table->cls[col_idx].is_bitset) {
        return;
    }

    uint64_t *words = (uint64_t*)table->cls[col_idx].data;
    ecs_bit_set bs = { .words = words };
    if (value) ecs_bitset_set(&bs, record->table_row);
    else ecs_bitset_unset(&bs, record->table_row);
}

bool ecs_get_bit(ecs_world_t *world, ecs_entity_t entity, ecs_component_t id) {
    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);

    int col_idx = ecs_table_get_column_index(table, id);
    if (col_idx == UINT16_MAX || col_idx >= table->type.count || !table->cls[col_idx].is_bitset) {
        return false;
    }

    uint64_t *words = (uint64_t*)table->cls[col_idx].data;
    ecs_bit_set bs = { .words = words };
    return ecs_bitset_is_set(&bs, record->table_row);
}

uint32_t ecs_query_init(ecs_world_t *world, const ecs_query_desc_t *desc) {
    return ecs_query_index_create(&world->query_index, desc);
}
