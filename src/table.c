#include "table.h"
#include "./storage/component_index.h"
#include "./type.h"
#include "datastructure/idmap.h"
#include "datastructure/vec.h"
#include <stdlib.h>

void ecs_table_init(
    ecs_table_t *table,
    ecs_type_t type,
    const ecs_component_index_t *component_index,
    uint16_t table_id
) {
    table->type = type;
    table->entity_capacity = 1;
    table->entity_count = 0;
    table->data_count = 0;
    table->entities = malloc(sizeof(ecs_entity_t) * table->entity_capacity);
    table->cls = type.count == 0 ? NULL : malloc(sizeof(ecs_column_t) * type.count);
    table->data_columns = type.count == 0 ? NULL : malloc(sizeof(uint16_t) * type.count);
    table->base_table_id = UINT16_MAX;
    table->bloom = ecs_type_bloom(&type);

    ecs_vec_init(&table->observers_by_event, sizeof(ecs_vec_t));
    ecs_id_map_init(&table->add_edge);

    for (uint16_t i = 0; i < type.count; i++) {
        ecs_component_record_t *rec = ecs_component_index_get_mut(component_index, type.ids[i]);
        ecs_vec_push_u16(&rec->tables, table_id);
        table->cls[i].size = rec->size;
        table->cls[i].data = rec->size != 0 ? calloc(table->entity_capacity, rec->size) : NULL;
        if (rec->size != 0) {
            table->data_columns[table->data_count++] = i;
        }
        ecs_id_map_set(&table->add_edge, type.ids[i], i);
        table->cls[i].remove_edge = UINT16_MAX;
    }

    if (table->data_count == 0) {
        free(table->data_columns);
        table->data_columns = NULL;
    } else if (table->data_count < type.count) {
        table->data_columns = realloc(table->data_columns, sizeof(uint16_t) * table->data_count);
    }
}

static inline void ecs_table_grow(ecs_table_t *table) {
    uint64_t new_capacity = table->entity_capacity * (uint64_t)2;
    table->entities = realloc(table->entities, sizeof(ecs_entity_t) * new_capacity);
    for (uint16_t i = 0; i < table->data_count; i++) {
        ecs_column_t *column = &table->cls[table->data_columns[i]];
        column->data = realloc(column->data, column->size * new_capacity);
        memset(
            (uint8_t *)column->data + (column->size * table->entity_capacity),
            0,
            column->size * (new_capacity - table->entity_capacity)
        );
    }
    table->entity_capacity = new_capacity;
}

uint32_t ecs_table_add_entity(ecs_table_t *table, ecs_entity_t entity) {
    if (ECS_UNLIKELY(table->entity_count >= table->entity_capacity)) {
        ecs_table_grow(table);
    }
    uint32_t row = table->entity_count++;
    table->entities[row] = entity;
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
        for (uint16_t i = 0; i < table->data_count; i++) {
            ecs_column_t *column = &table->cls[table->data_columns[i]];
            const void *src = (char *)column->data + (column->size * last_row);
            void *dst = (char *)column->data + (column->size * row);
            memcpy(dst, src, column->size);
        }
        table->entity_count -= 1;
        return moved_entity;
    }
    table->entity_count -= 1;
    return removed_entity;
}

void *ecs_table_get_component(ecs_table_t *table, ecs_component_t component_id, uint32_t row) {
    return ecs_table_component_at_column(
        table,
        ecs_table_get_column_index(table, component_id),
        row
    );
}

void ecs_table_add_observer(ecs_table_t *table, uint16_t event, uint16_t observer_id) {
    ecs_vec_ensure(&table->observers_by_event, event + 1, sizeof(ecs_vec_t));
    ecs_vec_t *list = ecs_vec_get_mut(&table->observers_by_event, event, ecs_vec_t);
    if (list->capacity == 0) {
        ecs_vec_init(list, sizeof(uint16_t));
    }
    ecs_vec_push_u16(list, observer_id);
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
    free(table->data_columns);
    ecs_type_fini(&table->type);
}
