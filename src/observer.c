#include "datastructure/vec.h"
#include "module.h"
#include "siecs.h"
#include "storage/observer_index.h"
#include "utils.h"
#include "world_internal.h"

ecs_event_t ecs_event(ecs_world_t *world) { return world->observer_index.event_count++; }

ecs_observer_id_t ecs_observer_init(ecs_world_t *world, const ecs_observer_desc_t *desc) {
    ecs_assert_not_null(world);
    ecs_assert(desc->callback != NULL, "Observer callback cannot be NULL");
    ecs_observer_id_t oid = ecs_observer_index_create(&world->observer_index, desc);
    ecs_observer_index_match_tables(
        &world->observer_index,
        world->table_index.tables,
        world->table_index.table_count,
        oid
    );
    ecs_module_record_observer(world, oid);
    return oid;
}

void ecs_observer_enable(ecs_world_t *world, ecs_observer_id_t id) {
    ecs_vec_get_mut(&world->observer_index.observers, id, ecs_observer_t)->enabled = true;
}

void ecs_observer_disable(ecs_world_t *world, ecs_observer_id_t id) {
    ecs_vec_get_mut(&world->observer_index.observers, id, ecs_observer_t)->enabled = false;
}

void ecs_observer_trigger(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_event_t event,
    const void *trigger_data
) {
    ecs_assert_not_null(world);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);
    ecs_emit(world, table, entity, event, trigger_data);
}
