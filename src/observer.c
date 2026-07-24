#include "datastructure/vec.h"
#include "module.h"
#include "siecs.h"
#include "storage/observer_index.h"
#include "utils.h"
#include "world_internal.h"

ecs_event_t ecs_event(void) { return ecs_world.observer_index.event_count++; }

ecs_event_t ecs_event_register(ecs_event_t *id) {
        ecs_assert_not_null(id);

    if (*id == UINT16_MAX) {
        *id = ecs_event();
        return *id;
    }

    if (ecs_world.observer_index.event_count <= *id) {
        ecs_world.observer_index.event_count = *id + 1;
    }

    return *id;
}

ecs_observer_id_t ecs_observer_init(const ecs_observer_desc_t *desc) {
        ecs_assert(desc->callback != NULL, "Observer callback cannot be NULL");
    ecs_observer_id_t oid = ecs_observer_index_create(&ecs_world.observer_index, desc);
    ecs_observer_index_match_tables(
                ecs_world.table_index.tables,
        ecs_world.table_index.table_count,
        oid
    );
    ecs_module_record_observer(oid);
    return oid;
}

void ecs_observer_enable(ecs_observer_id_t id) {
    ecs_vec_get_mut(&ecs_world.observer_index.observers, id, ecs_observer_t)->enabled = true;
}

void ecs_observer_disable(ecs_observer_id_t id) {
    ecs_vec_get_mut(&ecs_world.observer_index.observers, id, ecs_observer_t)->enabled = false;
}

void ecs_observer_trigger(
        ecs_entity_t entity,
    ecs_event_t event,
    const void *trigger_data
) {
        ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(entity);

    ecs_entity_record_t *record = ecs_get_record(entity);
    ecs_table_t *table = ecs_get_table(record->table_id);
    ecs_emit(table, entity, event, trigger_data);
}
