#include "table.h"
#include "./storage/component_index.h"
#include "./type.h"
#include "./world.h"
#include "ecs/datastructure/bitset.h"
#include "ecs/datastructure/idmap.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void ecs_table_init(
    ecs_table_t *table,
    ecs_type_t type,
    const ecs_component_index_t *component_index
) {
    table->type = type;
    table->entity_capacity = 1;
    table->entity_count = 0;
    table->entities = malloc(sizeof(ecs_entity_t) * table->entity_capacity);
    table->cls = malloc(sizeof(ecs_column_t) * type.count);
    table->bloom = ecs_type_bloom(&type);
    ecs_vec_init(&table->observers_by_event, sizeof(ecs_vec_t));
    ecs_id_map_init(&table->add_edge);

    for (uint16_t i = 0; i < type.count; i++) {
        const ecs_component_record_t *rec = ecs_component_index_get(component_index, type.ids[i]);
        table->cls[i].size = rec->size;
        table->cls[i].is_bitset = rec->is_bitset;

        if (rec->is_bitset) {
            uint32_t word_count = (table->entity_capacity + 63) / 64;
            table->cls[i].data = calloc(word_count, sizeof(uint64_t));
        } else if (rec->size != 0) {
            table->cls[i].data = malloc(rec->size * table->entity_capacity);
        } else {
            table->cls[i].data = NULL;
        }
        ecs_id_map_set(&table->add_edge, type.ids[i], i);
        table->cls[i].remove_edge = UINT16_MAX;
    }
}

static inline void ecs_table_grow(ecs_table_t *table) {
    uint64_t new_capacity = table->entity_capacity * 2;
    table->entities = realloc(table->entities, sizeof(ecs_entity_t) * new_capacity);
    for (uint16_t i = 0; i < table->type.count; i++) {
        if (table->cls[i].is_bitset) {
            uint32_t old_word_count = (table->entity_capacity + 63) / 64;
            uint32_t new_word_count = (new_capacity + 63) / 64;
            table->cls[i].data = realloc(table->cls[i].data, new_word_count * sizeof(uint64_t));
            memset(
                (uint8_t *)table->cls[i].data + (old_word_count * sizeof(uint64_t)),
                0,
                (new_word_count - old_word_count) * sizeof(uint64_t)
            );
        } else if (table->cls[i].size != 0) {
            table->cls[i].data = realloc(table->cls[i].data, table->cls[i].size * new_capacity);
        }
    }
    table->entity_capacity = new_capacity;
}

uint32_t ecs_table_add_entity(ecs_table_t *table, ecs_entity_t entity) {
    if (ECS_UNLIKELY(table->entity_count >= table->entity_capacity)) {
        ecs_table_grow(table);
    }
    uint32_t row = table->entity_count++;
    table->entities[row] = entity;
    for (uint16_t i = 0; i < table->type.count; i++) {
        if (table->cls[i].is_bitset) {
            ecs_bit_set bs = { .words = (uint64_t *)table->cls[i].data };
            ecs_bitset_unset(&bs, row);
        }
    }
    return row;
}

// if the entity is not the last one, the last entity will be moved to the removed entity's
// position, and the moved entity will be returned
ecs_entity_t ecs_table_remove_entity(ecs_table_t *table, uint32_t row) {
    ecs_entity_t removed_entity = table->entities[row];
    uint32_t last_row = table->entity_count - 1;
    if (row != last_row) {
        ecs_entity_t moved_entity = table->entities[last_row];
        table->entities[row] = moved_entity;
        for (uint16_t i = 0; i < table->type.count; i++) {
            if (table->cls[i].is_bitset) {
                uint64_t *words = (uint64_t *)table->cls[i].data;
                ecs_bit_set bs = { .words = words };
                bool val = ecs_bitset_is_set(&bs, last_row);
                if (val) {
                    ecs_bitset_set(&bs, row);
                } else {
                    ecs_bitset_unset(&bs, row);
                }
                ecs_bitset_unset(&bs, last_row);
            } else if (table->cls[i].size != 0) {
                void *src = (char *)table->cls[i].data + (table->cls[i].size * last_row);
                void *dst = (char *)table->cls[i].data + (table->cls[i].size * row);
                memcpy(dst, src, table->cls[i].size);
            }
        }
        table->entity_count -= 1;
        return moved_entity;
    }

    for (uint16_t i = 0; i < table->type.count; i++) {
        if (table->cls[i].is_bitset) {
            ecs_bit_set bs = { .words = (uint64_t *)table->cls[i].data };
            ecs_bitset_unset(&bs, last_row);
        }
    }
    table->entity_count -= 1;
    return removed_entity;
}

void *ecs_table_get_component(ecs_table_t *table, ecs_component_t component_id, uint32_t row) {
    ecs_column_t *column = &table->cls[ecs_table_get_column_index(table, component_id)];
    return (uint8_t *)column->data + ((column->size) * row);
}

void ecs_table_add_observer(ecs_table_t *table, uint16_t event, uint32_t observer_id) {
    ecs_vec_ensure(&table->observers_by_event, event + 1, sizeof(ecs_vec_t));
    ecs_vec_t *list = ecs_vec_get_mut(&table->observers_by_event, event, ecs_vec_t);
    if (list->capacity == 0) {
        ecs_vec_init(list, sizeof(uint32_t));
    }
    ecs_vec_push_u32(list, observer_id);
}

void ecs_table_fini(ecs_table_t *table) {
    for (uint16_t i = 0; i < table->type.count; i++) {
        free(table->cls[i].data);
    }
    for (uint32_t e = 0; e < table->observers_by_event.size; e++) {
        ecs_vec_fini(ecs_vec_get_mut(&table->observers_by_event, e, ecs_vec_t));
    }
    ecs_vec_fini(&table->observers_by_event);
    ecs_id_map_fini(&table->add_edge);
    free(table->entities);
    free(table->cls);
    ecs_type_fini(&table->type);
}
