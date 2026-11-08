#include "component_schema.h"
#include "pair.h"
#include "sireflect.h"
#include "table_ops.h"
#include "world_internal.h"
#include <stdlib.h>
#include <string.h>

typedef struct ecs_schema_column_t {
    uint16_t table;
    uint16_t column;
    uint32_t count;
    uint32_t capacity;
    void *previous;
    void *data;
    ecs_data_column_t *columns;
} ecs_schema_column_t;

struct ecs_component_schema_t {
    ecs_component_t component;
    sireflect_handle_t previous;
    sireflect_handle_t type;
    ecs_type_ops_t ops;
    sireflect_struct_desc_t *reflection;
    uint32_t count;
    uint32_t registered_tables;
    bool applied;
    ecs_pair_schema_t *pairs;
    ecs_schema_column_t columns[];
};

void ecs_component_schema_free(ecs_component_schema_t *schema) {
    if (!schema)
        return;
    for (uint32_t i = 0; i < schema->count; i++) {
        free(schema->columns[i].data);
        free(schema->columns[i].columns);
    }
    ecs_reflection_desc_free(schema->reflection);
    ecs_pair_schema_free(schema->pairs);
    free(schema);
}

ecs_component_schema_t *ecs_component_schema_prepare(
    ecs_component_t component,
    sireflect_handle_t type,
    const char *fields
) {
    if (!ecs_component_info(component) || !type || !fields || ecs_is_deferred())
        return NULL;
    const ecs_component_record_t *record = ecs_component_index_get(component);
    const sireflect_type_info_t *info = sireflect_type_info(type);
    if (!record->dynamic || record->override_target || !sireflect_type_is_struct(info) ||
        info->size > UINT32_MAX)
        return NULL;
    uint32_t count = record->tables.size;
    ecs_component_schema_t *schema =
        calloc(1, sizeof(*schema) + count * sizeof(ecs_schema_column_t));
    if (!schema)
        return NULL;
    schema->component = component;
    schema->registered_tables = count;
    schema->previous = record->info->type;
    schema->type = type;
    schema->ops = ecs_reflected_ops(type);
    if (!ecs_pair_schema_prepare(component, type, &schema->pairs)) {
        ecs_component_schema_free(schema);
        return NULL;
    }
    schema->reflection = calloc(1, sizeof(*schema->reflection));
    if (!schema->reflection) {
        ecs_component_schema_free(schema);
        return NULL;
    }
    schema->reflection->name = strdup(record->info->name);
    schema->reflection->fields = strdup(fields);
    schema->reflection->size = info->size;
    schema->reflection->align = info->align;
    if (!schema->reflection->name || !schema->reflection->fields) {
        ecs_component_schema_free(schema);
        return NULL;
    }
    const uint16_t *tables = record->tables.data;
    for (uint32_t i = 0; i < count; i++) {
        const ecs_table_t *table = ecs_get_table(tables[i]);
        uint16_t at = ecs_table_column_or_invalid(table, component);
        if (at == UINT16_MAX)
            continue;
        ecs_schema_column_t *column = &schema->columns[schema->count++];
        column->table = tables[i];
        column->column = at;
        column->count = table->entity_count;
        column->capacity = table->entity_capacity;
        column->previous = table->cls[column->column].data;
        if (info->size && column->capacity > SIZE_MAX / info->size) {
            ecs_component_schema_free(schema);
            return NULL;
        }
        column->data = info->size ? calloc(column->capacity, info->size) : NULL;
        column->columns = malloc(table->type.component_count * sizeof(ecs_data_column_t));
        if ((info->size && !column->data) || !column->columns) {
            ecs_component_schema_free(schema);
            return NULL;
        }
    }
    return schema;
}

bool ecs_component_schema_validate(const ecs_component_schema_t *schema) {
    const ecs_component_record_t *record = ecs_component_index_get(schema->component);
    if (schema->applied || !ecs_pair_schema_validate(schema->pairs) ||
        record->info->type != schema->previous ||
        record->tables.size != schema->registered_tables || ecs_is_deferred())
        return false;
    for (uint32_t i = 0; i < schema->count; i++) {
        const ecs_schema_column_t *column = &schema->columns[i];
        const ecs_table_t *table = ecs_get_table(column->table);
        if (table->entity_count != column->count || table->entity_capacity != column->capacity ||
            table->cls[column->column].data != column->previous)
            return false;
    }
    return true;
}

void ecs_component_schema_apply(ecs_component_schema_t *schema) {
    ecs_pair_schema_apply(schema->pairs);
    ecs_component_record_t *record = ecs_component_index_get(schema->component);
    uint32_t size = (uint32_t)sireflect_type_size(schema->type);
    ecs_type_ops_t previous = record->ops;
    for (uint32_t i = 0; i < schema->count; i++) {
        ecs_schema_column_t *column = &schema->columns[i];
        ecs_table_t *table = ecs_get_table(column->table);
        ecs_value_fini(&previous, column->previous, column->count);
        free(column->previous);
        table->cls[column->column].data = column->data;
        table->cls[column->column].size = size;
        table->cls[column->column].flags = ecs_column_value_flags(size, &schema->ops);
        ecs_table_column_changed(table, column->column, ecs_change_tick());
        column->data = NULL;
        ecs_table_rebind_data_columns(table, column->columns);
        column->columns = NULL;
    }
    ecs_reflection_desc_free(record->info->reflection);
    record->info->reflection = schema->reflection;
    schema->reflection = NULL;
    record->info->type = schema->type;
    record->info->size = size;
    record->size = size;
    record->ops = schema->ops;
    record->plain = size && !schema->ops.reflected_type;
    for (uint32_t i = 0; i < schema->count; i++)
        ecs_query_index_refresh_table_fields(ecs_get_table(schema->columns[i].table));
    system_index.plan_dirty = true;
    schema->applied = true;
}

void ecs_component_schema_notify(
    const ecs_component_schema_t *schema,
    bool (*already_written)(ecs_entity_t, ecs_component_t, void *),
    void *context
) {
    ecs_pair_schema_notify(schema->pairs);
    for (uint32_t i = 0; i < schema->count; i++) {
        ecs_table_t *table = ecs_get_table(schema->columns[i].table);
        uint16_t column = schema->columns[i].column;
        for (uint32_t row = 0; row < table->entity_count; row++) {
            if (already_written &&
                already_written(table->entities[row], schema->component, context))
                continue;
            ecs_emit(
                table,
                table->entities[row],
                EcsOnSet,
                schema->component,
                ecs_table_component_at_column(table, column, row)
            );
        }
    }
}

bool ecs_component_dynamic_redefine(ecs_component_t component, const char *fields) {
    if (!ecs_component_info(component) || !fields || ecs_is_deferred() ||
        !ecs_component_index_get(component)->dynamic)
        return false;
    sireflect_transaction_t *transaction = sireflect_transaction_begin();
    if (!transaction)
        return false;
    sireflect_handle_t type =
        sireflect_try_replace_dynamic_struct(ecs_component_name(component), fields);
    ecs_component_schema_t *schema =
        type ? ecs_component_schema_prepare(component, type, fields) : NULL;
    if (!schema || !ecs_component_schema_validate(schema) ||
        !sireflect_transaction_commit(transaction)) {
        ecs_component_schema_free(schema);
        sireflect_transaction_abort(transaction);
        return false;
    }
    ecs_sync_begin();
    ecs_observer_batch_begin();
    ecs_component_schema_apply(schema);
    ecs_component_schema_notify(schema, NULL, NULL);
    ecs_observer_batch_end();
    ecs_component_schema_free(schema);
    ecs_sync_end();
    return true;
}
