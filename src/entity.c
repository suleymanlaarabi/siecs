#include "siecs.h"
#include "storage/component_index.h"
#include "table.h"
#include "utils.h"
#include "world_internal.h"

ecs_entity_t ecs_new(ecs_world_t *world) {
    ecs_assert_not_null(world);
    ecs_table_t *table = ecs_get_table(world, 0);

    ecs_entity_t entity = ecs_entity_index_create(&world->entity_index, table->entity_count);
    ecs_table_add_entity(table, entity);

    return entity;
}

int ecs_is_alive(const ecs_world_t *world, ecs_entity_t entity) {
    return ecs_entity_index_is_alive(&world->entity_index, entity);
}

void ecs_kill(ecs_world_t *world, ecs_entity_t entity) {
    ecs_assert_not_null(world);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);

    for (int i = 0; i < table->type.count; i++) {
        const ecs_component_record_t *crec =
            ecs_component_index_get(&world->component_index, table->type.ids[i]);

        if (crec->on_remove) {
            crec->on_remove(
                world,
                entity,
                table->type.ids[i],
                ecs_table_get_component(table, table->type.ids[i], record->table_row)
            );
        }
    }

    // Remove from table
    ecs_entity_t moved = ecs_table_remove_entity(table, record->table_row);
    if (moved != entity) {
        ecs_get_record(world, moved)->table_row = record->table_row;
    }

    ecs_entity_index_kill(&world->entity_index, ecs_first(entity));
}
