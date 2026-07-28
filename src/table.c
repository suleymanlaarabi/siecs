#include "table.h"
#include "./storage/component_index.h"
#include "./type.h"
#include "datastructure/idmap.h"
#include "sicore_vec.h"
#include "utils.h"
#include "world_internal.h"
#include <stdlib.h>
#include <string.h>

void ecs_table_init(ecs_table_t *table, ecs_type_t type, uint16_t table_id) {
    table->type = type;
    table->entity_capacity = 1;
    table->entity_count = 0;
    // data_count belongs to the canonical table layout, not to transient types.
    table->type.data_count = 0;
    table->entities = malloc(sizeof(ecs_entity_t) * table->entity_capacity);
    table->cls = type.count == 0 ? NULL : malloc(sizeof(ecs_column_t) * type.count);
    table->data_columns = type.count == 0 ? NULL : malloc(sizeof(uint16_t) * type.count);
    table->bloom = ecs_type_bloom(&type);

    sicore_vec_init(&table->observers_by_event, sizeof(sicore_vec_t));
    ecs_id_map_init(&table->add_edge);

    for (uint16_t i = 0; i < type.count; i++) {
        ecs_component_record_t *rec = ecs_component_index_get_mut(type.ids[i]);
        sicore_vec_push_u16(&rec->tables, table_id);
        table->cls[i].size = rec->size;
        table->cls[i].data = rec->size != 0 ? calloc(table->entity_capacity, rec->size) : NULL;
        if (rec->size != 0) {
            table->data_columns[table->type.data_count++] = i;
        }
        ecs_id_map_set(&table->add_edge, type.ids[i], i);
        table->cls[i].remove_edge = UINT16_MAX;
        table->cls[i].flags = 0;
        if (rec->size == 0 || (!rec->ops.move_ctor && !rec->ops.copy_ctor)) {
            table->cls[i].flags |= EcsColumnTrivialMove;
        }
        if (rec->size == 0 || !rec->ops.dtor) {
            table->cls[i].flags |= EcsColumnNoDtor;
        }
        if (rec->size == 0 || !rec->ops.ctor) {
            table->cls[i].flags |= EcsColumnZeroCtor;
        }
    }

    if (table->type.data_count == 0) {
        free(table->data_columns);
        table->data_columns = NULL;
    } else if (table->type.data_count < type.count) {
        table->data_columns =
            realloc(table->data_columns, sizeof(uint16_t) * table->type.data_count);
    }
}

static inline void ecs_table_grow(ecs_table_t *table) {
    uint64_t new_capacity = table->entity_capacity * (uint64_t)2;
    table->entities = realloc(table->entities, sizeof(ecs_entity_t) * new_capacity);
    for (uint16_t i = 0; i < table->type.data_count; i++) {
        uint16_t column_index = table->data_columns[i];
        ecs_column_t *column = &table->cls[column_index];

        if (column->flags & EcsColumnTrivialMove) {
            void *new_data = realloc(column->data, (size_t)new_capacity * column->size);
            ecs_assert_not_null(new_data);
            column->data = new_data;
            continue;
        }

        const ecs_component_record_t *record =
            ecs_component_index_get(table->type.ids[column_index]);
        void *new_data = malloc((size_t)new_capacity * column->size);
        ecs_assert_not_null(new_data);
        ecs_component_value_move_ctor(record, new_data, column->data, table->entity_count);
        free(column->data);
        column->data = new_data;
    }
    table->entity_capacity = new_capacity;
    ecs_query_index_refresh_table_fields(table, (uint16_t)(table - ecs_world.table_index.tables));
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
ecs_entity_t ecs_table_remove_entity(ecs_table_t *table, uint32_t row, bool row_values_live) {
    ecs_entity_t removed_entity = table->entities[row];
    uint32_t last_row = table->entity_count - 1;
    if (row_values_live) {
        for (uint16_t i = 0; i < table->type.data_count; i++) {
            uint16_t column_index = table->data_columns[i];
            ecs_column_t *column = &table->cls[column_index];
            if (column->flags & EcsColumnNoDtor) {
                continue;
            }
            const ecs_component_record_t *record =
                ecs_component_index_get(table->type.ids[column_index]);
            void *ptr = (char *)column->data + (column->size * row);
            ecs_component_value_dtor(record, ptr, 1);
        }
    }
    if (row != last_row) {
        ecs_entity_t moved_entity = table->entities[last_row];
        table->entities[row] = moved_entity;
        for (uint16_t i = 0; i < table->type.data_count; i++) {
            uint16_t column_index = table->data_columns[i];
            ecs_column_t *column = &table->cls[column_index];
            void *src = (char *)column->data + (column->size * last_row);
            void *dst = (char *)column->data + (column->size * row);
            if (column->flags & EcsColumnTrivialMove) {
                memcpy(dst, src, column->size);
                continue;
            }
            const ecs_component_record_t *record =
                ecs_component_index_get(table->type.ids[column_index]);
            ecs_component_value_move_ctor(record, dst, src, 1);
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
    sicore_vec_ensure(&table->observers_by_event, event + 1, sizeof(sicore_vec_t));
    sicore_vec_t *list = sicore_vec_get_mut(&table->observers_by_event, event, sicore_vec_t);
    if (list->capacity == 0) {
        sicore_vec_init(list, sizeof(uint16_t));
    }
    sicore_vec_push_u16(list, observer_id);
}

static void ecs_table_fini_component_values(ecs_table_t *table) {
    for (uint16_t c = 0; c < table->type.count; c++) {
        ecs_component_t component = table->type.ids[c];
        const ecs_component_record_t *crec = ecs_component_index_get(component);

        if (crec->relation_flags & EcsRelationSource) {
            if (!(crec->relation_flags & EcsRelationOneToOne)) {
                for (uint32_t row = 0; row < table->entity_count; row++) {
                    RelationSource *source = ecs_table_component_at_column(table, c, row);
                    sicore_vec_fini(&source->entities);
                }
            }
            continue;
        }

        if (crec->relation_flags & EcsRelationTarget) {
            continue;
        }

        for (uint32_t row = 0; row < table->entity_count; row++) {
            void *ptr = ecs_table_component_at_column(table, c, row);
            if (crec->on_remove) {
                crec->on_remove(table->entities[row], component, ptr);
            }
            ecs_component_value_dtor(crec, ptr, 1);
        }
    }
}

void ecs_table_fini(ecs_table_t *table) {
    ecs_table_fini_component_values(table);

    for (uint16_t i = 0; i < table->type.count; i++) {
        free(table->cls[i].data);
    }
    for (uint32_t e = 0; e < table->observers_by_event.size; e++) {
        sicore_vec_fini(sicore_vec_get_mut(&table->observers_by_event, e, sicore_vec_t));
    }
    sicore_vec_fini(&table->observers_by_event);
    ecs_id_map_fini(&table->add_edge);
    free(table->entities);
    free(table->cls);
    free(table->data_columns);
    ecs_type_fini(&table->type);
}

bool ecs_table_has(const ecs_table_t *table, ecs_component_t component_id) {
    if (ecs_table_column_or_invalid(table, component_id) != UINT16_MAX) {
        return true;
    }

    if (component_id == ecs_id(Abstract)) {
        return false;
    }

    ecs_entity_t base = table->type.base;
    while (base != 0) {
        const ecs_entity_record_t *record = ecs_get_record(base);
        const ecs_table_t *base_table = ecs_get_table(record->table_id);
        if (ecs_table_column_or_invalid(base_table, component_id) != UINT16_MAX) {
            return true;
        }
        base = base_table->type.base;
    }

    return false;
}

bool ecs_table_is_a(const ecs_table_t *table, ecs_entity_t base) {
    if (base == 0) {
        return true;
    }

    ecs_entity_t current = table->type.base;
    while (current != 0) {
        if (current == base) {
            return true;
        }

        const ecs_entity_record_t *record = ecs_get_record(current);
        const ecs_table_t *base_table = ecs_get_table(record->table_id);
        current = base_table->type.base;
    }

    return false;
}

void *ecs_table_field(const ecs_table_t *table, ecs_component_t component_id, bool *is_shared) {
    uint16_t cidx = ecs_table_column_or_invalid(table, component_id);
    if (cidx != UINT16_MAX) {
        *is_shared = false;
        return table->cls[cidx].data;
    }

    ecs_entity_t base = table->type.base;
    while (base != 0) {
        const ecs_entity_record_t *record = ecs_get_record(base);
        const ecs_table_t *base_table = ecs_get_table(record->table_id);

        cidx = ecs_table_column_or_invalid(base_table, component_id);
        if (cidx != UINT16_MAX) {
            *is_shared = true;
            return ecs_table_component_at_column(base_table, cidx, record->table_row);
        }

        base = base_table->type.base;
    }

    *is_shared = false;
    return NULL;
}
