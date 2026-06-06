#include "ecs/utils.h"
#include "ecs/world.h"
#include "ecs/world_internal.h"

ecs_event_t ecs_event(ecs_world_t *world) { return world->observer_index.event_count++; }

uint32_t ecs_observer_init(ecs_world_t *world, const ecs_observer_desc_t *desc) {
    ecs_assert_not_null(world);
    ecs_assert(desc->callback != NULL, "Observer callback cannot be NULL");
    uint32_t oid = ecs_observer_index_create(&world->observer_index, desc);
    ecs_observer_index_match_tables(
        &world->observer_index,
        world->table_index.tables,
        world->table_index.table_count,
        oid
    );
    return oid;
}

void ecs_observer_trigger(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_event_t event,
    void *trigger_data
) {
    ecs_assert_not_null(world);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);
    ecs_emit(world, table, entity, event, trigger_data);
}
