#include "./id.h"
#include "compiler.h"
#include "datastructure/idmap.h"
#include "siecs.h"
#include "sihttp.h"
#include "sijson.h"
#include "sireflect.h"
#include "storage/component_index.h"
#include "storage/entity_index.h"
#include "storage/query_index.h"
#include "storage/system_index.h"
#include "storage/table_index.h"
#include "table.h"
#include "type.h"
#include "utils.h"
#include "world_internal.h"
#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef SIECS_REST

sihttp_response_t get_entities(const sihttp_request_t *req) {
    ecs_world_t *world = req->state->world;

    sijson_value_t array = sijson_make_array();

    const uint64_t count = world->entity_index.entities.size;
    const ecs_entity_record_t *records = world->entity_index.entities.data;

    for (uint64_t i = 0; i < count; i++) {
        sijson_array_push(
            array,
            sijson_make_string(siformat("(%ld, %d)", i, records[i].generation))
        );
    }

    return sihttp_response(
        {
            .status = 200,
            .body = sijson_stringify(array),
            .content_type = SIHTTP_CONTENT_JSON,
        }
    );
}

#endif

ecs_world_t *ecs_init() {
    ecs_world_t *world = malloc(sizeof(ecs_world_t));
    ecs_entity_index_init(&world->entity_index);
    ecs_component_index_init(&world->component_index);
    ecs_table_index_init(&world->table_index);
    ecs_query_index_init(&world->query_index);
    ecs_observer_index_init(&world->observer_index);
    ecs_system_index_init(&world->system_index);

    world->sireflect_registry = sijson_default_registry();

    ecs_bootstrap(world);

#ifdef SIECS_REST
    sihttp_app_state_t *state = malloc(sizeof(sihttp_app_state_t));

    state->world = world;

    world->server = sihttp_server(
        {
            .port = 4040,
            .state = state,
        }
    );

    sihttp_get(world->server, "/entities", get_entities);

    sihttp_server_start(world->server);
#endif

    return world;
}

static inline void
copy_column(ecs_column_t *from, uint32_t from_row, ecs_column_t *to, uint32_t to_row) {
    memcpy(
        (uint8_t *)to->data + (from->size * to_row),
        (uint8_t *)from->data + (from->size * from_row),
        from->size
    );
}

// Generic migration: move an entity from its current table to an arbitrary
// target table, without knowing which components were added or removed, or how
// many. Both type id arrays are sorted ascending, so a sorted merge classifies
// each column: shared -> copy, only-in-from -> removed (skip), only-in-to ->
// added (zero). Pure data movement; callers own events/hooks.
static inline void migrate_entity(
    ecs_world_t *world,
    ecs_entity_record_t *record,
    ecs_entity_t entity,
    ecs_table_t *from_table,
    uint16_t to_id
) {
    ecs_table_t *to_table = ecs_get_table(world, to_id);

    uint32_t old_row = record->table_row;
    uint32_t new_row = ecs_table_add_entity(to_table, entity);

    uint16_t fi = 0, ti = 0;
    while (fi < from_table->type.count && ti < to_table->type.count) {
        uint16_t fid = from_table->type.ids[fi];
        uint16_t tid = to_table->type.ids[ti];
        if (fid == tid) {
            copy_column(&from_table->cls[fi], old_row, &to_table->cls[ti], new_row);
            fi++;
            ti++;
        } else if (fid < tid) {
            fi++;
        } else {
            ecs_column_t *c = &to_table->cls[ti];
            if (c->size != 0)
                memset((uint8_t *)c->data + (c->size * new_row), 0, c->size);
            ti++;
        }
    }
    for (; ti < to_table->type.count; ti++) {
        ecs_column_t *c = &to_table->cls[ti];
        if (c->size != 0)
            memset((uint8_t *)c->data + (c->size * new_row), 0, c->size);
    }

    ecs_entity_t moved = ecs_table_remove_entity(from_table, old_row);
    if (moved != entity)
        ecs_get_record(world, moved)->table_row = old_row;

    record->table_id = to_id;
    record->table_row = new_row;
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
    if (to_table->cls[k].size != 0) {
        memset(
            (uint8_t *)to_table->cls[k].data + (to_table->cls[k].size * new_row),
            0,
            to_table->cls[k].size
        );
    }
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

void ecs_add_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    uint16_t from_id = record->table_id;
    ecs_table_t *table = ecs_get_table(world, from_id);

    if (ecs_table_has(table, cid)) {
        return;
    }

    const ecs_component_record_t *crec = ecs_component_index_get(&world->component_index, cid);
    for (uint32_t i = 0; i < crec->required_count; i++) {
        ecs_add_cid(world, entity, crec->required[i]);
    }

    record = ecs_get_record(world, entity);
    from_id = record->table_id;
    table = ecs_get_table(world, from_id);

    if (ecs_table_has(table, cid)) {
        return;
    }

    uint16_t new_table_id = ecs_table_get_add_edge(table, cid);

    if (new_table_id == UINT16_MAX) {
        ecs_type_t new_type = ecs_type_with_add(&table->type, cid);
        new_table_id = ecs_table_index_get_or_create(world, new_type);

        // Re-fetch: ecs_table_index_get_or_create may realloc the tables vec
        table = ecs_get_table(world, from_id);
        ecs_id_map_set(&table->add_edge, cid, new_table_id);
    } else if (
        ECS_UNLIKELY(new_table_id < table->type.count && table->type.ids[new_table_id] == cid)
    ) {
        return;
    }

    migrate_entity_add(world, record, entity, table, new_table_id, cid);

    ecs_table_t *new_table = ecs_get_table(world, new_table_id);
    const void *component_data = ecs_table_get_component(new_table, cid, record->table_row);
    ecs_emit(world, new_table, entity, OnAdd, component_data);
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

    if (ECS_UNLIKELY(
            col_idx == UINT16_MAX || col_idx >= table->type.count || table->type.ids[col_idx] != cid
        )) {
        return;
    }

    uint16_t new_table_id = table->cls[col_idx].remove_edge;
    if (new_table_id == UINT16_MAX) {
        ecs_type_t new_type = ecs_type_with_remove(&table->type, cid);
        new_table_id = ecs_table_index_get_or_create(world, new_type);
        // Re-fetch: ecs_table_index_get_or_create may realloc the tables vec
        table = ecs_get_table(world, from_id);
        table->cls[col_idx].remove_edge = new_table_id;
    }

    void *removed_data = ecs_table_get_component(table, cid, record->table_row);

    const ecs_component_record_t *crec = ecs_component_index_get(&world->component_index, cid);
    if (crec->on_remove) {
        crec->on_remove(world, entity, cid, removed_data);
    }
    ecs_emit(world, table, entity, OnRemove, removed_data);

    migrate_entity_remove(world, record, entity, table, new_table_id, (uint16_t)col_idx);
}

void *ecs_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    const ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);
    return ecs_table_get_component(table, cid, record->table_row);
}

void *ecs_try_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    const ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);

    if (ecs_table_has(table, cid)) {
        return ecs_table_get_component(table, cid, record->table_row);
    }
    return NULL;
}

void ecs_set_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid, const void *data) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_add_cid(world, entity, cid);
    void *dst = ecs_get_cid(world, entity, cid);
    const ecs_component_record_t *crec = ecs_component_index_get(&world->component_index, cid);
    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);

    // on_set sees the new input data, while the table still stores the old data.
    // Hooks that need both can use ptr for new data and ecs_get_cid for old data.
    if (crec->on_set) {
        crec->on_set(world, entity, cid, data);
    }
    ecs_emit(world, table, entity, OnSet, data);
    memcpy(dst, data, crec->size);
}

bool ecs_has_cid(const ecs_world_t *world, ecs_entity_t entity, ecs_component_t id) {
    ecs_assert_not_null(world);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    uint16_t tid = ecs_get_record(world, entity)->table_id;
    return ecs_table_has(ecs_get_table(world, tid), id);
}

#ifndef NDEBUG
static bool ecs_component_requires(
    const ecs_world_t *world,
    ecs_component_t component,
    ecs_component_t require
) {
    const ecs_component_record_t *record =
        ecs_component_index_get(&world->component_index, component);

    for (uint32_t i = 0; i < record->required_count; i++) {
        ecs_component_t current = record->required[i];
        if (current == require || ecs_component_requires(world, current, require)) {
            return true;
        }
    }

    return false;
}
#endif

void ecs_with(ecs_world_t *world, ecs_component_t component, ecs_component_t require) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(component);
    ecs_assert_id_valid(require);
    ecs_assert(component != require, "component cannot require itself: %d\n", component);
    ecs_assert(
        !ecs_component_requires(world, require, component),
        "cyclic component requirement: %d requires %d\n",
        component,
        require
    );

    ecs_component_record_t *record =
        ecs_component_index_get_mut(&world->component_index, component);

    for (uint32_t i = 0; i < record->required_count; i++) {
        if (record->required[i] == require) {
            return;
        }
    }

    record->required =
        realloc(record->required, sizeof(ecs_component_t) * (record->required_count + 1));
    record->required[record->required_count++] = require;
}

void ecs_fini(ecs_world_t *world) {
    ecs_entity_index_fini(&world->entity_index);
    ecs_component_index_fini(&world->component_index);
    ecs_table_index_fini(&world->table_index);
    ecs_query_index_fini(&world->query_index);
    ecs_observer_index_fini(&world->observer_index);
    ecs_system_index_fini(&world->system_index);
    sireflect_registry_fini(world->sireflect_registry);

#ifdef SIECS_REST
    sihttp_server_stop(world->server);
    sihttp_server_fini(world->server);
#endif

    free(world);
}

void ecs_clone_w_entity(ecs_world_t *world, ecs_entity_t entity, ecs_entity_t target) {
    const ecs_entity_record_t *target_record = ecs_get_record(world, target);
    ecs_table_t *target_table = ecs_get_table(world, target_record->table_id);

    ecs_entity_record_t *entity_record = ecs_get_record(world, entity);
    ecs_table_t *entity_table = ecs_get_table(world, entity_record->table_id);

    ecs_table_add_entity(target_table, entity);

    migrate_entity(world, entity_record, entity, entity_table, target_record->table_id);
}
