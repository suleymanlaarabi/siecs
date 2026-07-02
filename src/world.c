#include "./id.h"
#include "compiler.h"
#include "datastructure/idmap.h"
#include "siecs.h"
#ifndef SIHTTP_H
#include "sihttp.h"
#endif
#ifndef SIREFLECT_H
#include "sireflect.h"
#endif
#include "storage/component_index.h"
#include "storage/entity_index.h"
#include "storage/module_index.h"
#include "storage/query_index.h"
#include "storage/resource_index.h"
#include "storage/system_index.h"
#include "storage/table_index.h"
#include "table.h"
#include "type.h"
#include "utils.h"
#include "world_internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ecs_assert_can_be_updated(world, entity, ...)                                              \
    ecs_assert(!ecs_has_cid_owned(world, entity, ecs_id(Abstract)), __VA_ARGS__)

ecs_world_t *ecs_init_w_features(const ecs_world_feat_desc_t *features) {
    ecs_world_t *world = malloc(sizeof(ecs_world_t));
    ecs_entity_index_init(&world->entity_index);
    ecs_component_index_init(&world->component_index);
    ecs_table_index_init(&world->table_index);
    ecs_query_index_init(&world->query_index);
    ecs_observer_index_init(&world->observer_index);
    ecs_system_index_init(&world->system_index);
    ecs_module_index_init(&world->module_index);
    ecs_resource_index_init(&world->resource_index);
    ecs_arena_init(&world->arena_allocator);
    world->active_module = 0;
    world->features = *features;
    world->did_start = false;
    world->exit = false;
    world->server = NULL;

    world->sireflect_registry = sireflect_registry_init();

    ecs_bootstrap(world);
    return world;
}

ecs_world_t *ecs_init() {
    ecs_world_t *world = ecs_with_features({});

    return world;
}

static inline void copy_column(
    const ecs_column_t *restrict from,
    const uint32_t from_row,
    ecs_column_t *restrict to,
    const uint32_t to_row
) {
    if (from->size == 0)
        return;
    memcpy(
        (uint8_t *)to->data + (from->size * to_row),
        (uint8_t *)from->data + (from->size * from_row),
        from->size
    );
}

static inline void finish_migration(
    const ecs_world_t *world,
    ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    const uint32_t old_row,
    const uint16_t to_table_id,
    const uint32_t new_row
) {
    ecs_entity_t moved = ecs_table_remove_entity(from_table, old_row);
    if (moved != entity) {
        ecs_get_record(world, moved)->table_row = old_row;
    }

    record->table_id = to_table_id;
    record->table_row = new_row;
}

// Generic migration: move an entity from its current table to an arbitrary
// target table, without knowing which components were added or removed, or how
// many. Both type id arrays are sorted ascending, so a sorted merge classifies
// each column: shared -> copy, only-in-from -> removed (skip), only-in-to ->
// added (zero). Pure data movement; callers own events/hooks.
static inline void migrate_entity(
    ecs_world_t *world,
    ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    const uint16_t to_id
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

    finish_migration(world, record, entity, from_table, old_row, to_id, new_row);
}

static inline void *migrate_entity_add(
    const ecs_world_t *world,
    ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    ecs_table_t *to_table,
    const uint16_t to_table_id,
    const ecs_component_t added_id
) {
    const uint32_t old_row = record->table_row;
    const uint32_t new_row = ecs_table_add_entity(to_table, entity);

    const uint16_t k = ecs_table_get_column_index(to_table, added_id);
    ecs_column_t *added = &to_table->cls[k];
    if (added->size != 0) {
        memset((uint8_t *)added->data + (added->size * new_row), 0, added->size);
    }

    uint16_t i = 0;
    for (; i < from_table->data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        if (from_col >= k) {
            break;
        }
        copy_data_column(&from_table->cls[from_col], old_row, &to_table->cls[from_col], new_row);
    }
    for (; i < from_table->data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        copy_data_column(
            &from_table->cls[from_col],
            old_row,
            &to_table->cls[from_col + 1],
            new_row
        );
    }

    finish_migration(world, record, entity, from_table, old_row, to_table_id, new_row);
    return ecs_table_component_at_column(to_table, k, new_row);
}

static inline void *migrate_entity_add_many(
    const ecs_world_t *world,
    ecs_entity_record_t *record,
    const ecs_entity_t entity,
    ecs_table_t *from_table,
    ecs_table_t *to_table,
    const uint16_t to_table_id,
    const ecs_component_t requested_id
) {
    const uint32_t old_row = record->table_row;
    const uint32_t new_row = ecs_table_add_entity(to_table, entity);

    uint16_t from_data = 0;
    for (uint16_t to_data = 0; to_data < to_table->data_count; to_data++) {
        const uint16_t to_col = to_table->data_columns[to_data];
        const ecs_component_t to_id = to_table->type.ids[to_col];

        while (from_data < from_table->data_count) {
            const uint16_t from_col = from_table->data_columns[from_data];
            const ecs_component_t from_id = from_table->type.ids[from_col];
            if (from_id >= to_id) {
                break;
            }
            from_data++;
        }

        if (from_data < from_table->data_count) {
            const uint16_t from_col = from_table->data_columns[from_data];
            if (from_table->type.ids[from_col] == to_id) {
                copy_data_column(
                    &from_table->cls[from_col],
                    old_row,
                    &to_table->cls[to_col],
                    new_row
                );
                from_data++;
                continue;
            }
        }

        ecs_column_t *column = &to_table->cls[to_col];
        memset((uint8_t *)column->data + (column->size * new_row), 0, column->size);
    }

    finish_migration(world, record, entity, from_table, old_row, to_table_id, new_row);
    return ecs_table_component_at_column(
        to_table,
        ecs_table_get_column_index(to_table, requested_id),
        new_row
    );
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

    uint16_t i = 0;
    for (; i < from_table->data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        if (from_col >= col_idx) {
            break;
        }
        copy_data_column(&from_table->cls[from_col], old_row, &to_table->cls[from_col], new_row);
    }
    if (i < from_table->data_count && from_table->data_columns[i] == col_idx) {
        i++;
    }
    for (; i < from_table->data_count; i++) {
        uint16_t from_col = from_table->data_columns[i];
        copy_data_column(
            &from_table->cls[from_col],
            old_row,
            &to_table->cls[from_col - 1],
            new_row
        );
    }

    finish_migration(world, record, entity, from_table, old_row, to_id, new_row);
}

typedef struct {
    ecs_type_t type;
    ecs_component_t inline_added[32];
    ecs_component_t *added;
    uint16_t added_count;
    uint16_t added_capacity;
} ecs_add_plan_t;

static inline void ecs_add_plan_init(ecs_add_plan_t *plan) {
    plan->type = (ecs_type_t){ 0 };
    plan->added = plan->inline_added;
    plan->added_count = 0;
    plan->added_capacity = 32;
}

static inline void ecs_add_plan_fini(ecs_add_plan_t *plan) {
    if (plan->added != plan->inline_added) {
        free(plan->added);
    }
}

static inline bool ecs_add_plan_has(const ecs_add_plan_t *plan, ecs_component_t id) {
    for (uint16_t i = 0; i < plan->added_count; i++) {
        if (plan->added[i] == id) {
            return true;
        }
    }
    return false;
}

static inline void ecs_add_plan_push(ecs_add_plan_t *plan, ecs_component_t id) {
    if (plan->added_count == plan->added_capacity) {
        const uint16_t old_capacity = plan->added_capacity;
        plan->added_capacity *= 2;
        if (plan->added == plan->inline_added) {
            plan->added = malloc(sizeof(ecs_component_t) * plan->added_capacity);
            memcpy(plan->added, plan->inline_added, sizeof(ecs_component_t) * old_capacity);
        } else {
            plan->added = realloc(plan->added, sizeof(ecs_component_t) * plan->added_capacity);
        }
    }
    plan->added[plan->added_count++] = id;
}

static void ecs_add_plan_collect_requirements(
    ecs_world_t *world,
    ecs_table_t *from_table,
    ecs_add_plan_t *plan,
    const ecs_component_record_t *crec
) {
    for (uint32_t i = 0; i < crec->required_count; i++) {
        ecs_component_t required = crec->required[i];
        if (ecs_table_has_owned(from_table, required) || ecs_add_plan_has(plan, required)) {
            continue;
        }

        const ecs_component_record_t *required_rec =
            ecs_component_index_get(&world->component_index, required);
        if (required_rec->required_count) {
            ecs_add_plan_collect_requirements(world, from_table, plan, required_rec);
        }
        ecs_add_plan_push(plan, required);
    }
}

static inline void ecs_sort_component_ids(ecs_component_t *ids, uint16_t count) {
    for (uint16_t i = 1; i < count; i++) {
        ecs_component_t id = ids[i];
        uint16_t j = i;
        while (j > 0 && ids[j - 1] > id) {
            ids[j] = ids[j - 1];
            j--;
        }
        ids[j] = id;
    }
}

static void ecs_add_plan_build_type(
    ecs_world_t *world,
    ecs_table_t *from_table,
    ecs_component_t cid,
    const ecs_component_record_t *crec,
    ecs_add_plan_t *plan
) {
    ecs_add_plan_init(plan);
    ecs_add_plan_collect_requirements(world, from_table, plan, crec);
    ecs_add_plan_push(plan, cid);

    ecs_component_t inline_sorted[32];
    ecs_component_t *sorted = plan->added_count <= 32
                                  ? inline_sorted
                                  : malloc(sizeof(ecs_component_t) * plan->added_count);
    memcpy(sorted, plan->added, sizeof(ecs_component_t) * plan->added_count);
    ecs_sort_component_ids(sorted, plan->added_count);

    ecs_type_t type = {
        .ids = malloc(sizeof(ecs_component_t) * (from_table->type.count + plan->added_count)),
        .count = from_table->type.count + plan->added_count,
        .base = from_table->type.base,
    };

    uint16_t from_i = 0;
    uint16_t add_i = 0;
    uint16_t out_i = 0;
    while (from_i < from_table->type.count && add_i < plan->added_count) {
        ecs_component_t from_id = from_table->type.ids[from_i];
        ecs_component_t add_id = sorted[add_i];
        if (from_id < add_id) {
            type.ids[out_i++] = from_id;
            from_i++;
        } else {
            type.ids[out_i++] = add_id;
            add_i++;
        }
    }
    while (from_i < from_table->type.count) {
        type.ids[out_i++] = from_table->type.ids[from_i++];
    }
    while (add_i < plan->added_count) {
        type.ids[out_i++] = sorted[add_i++];
    }

    if (sorted != inline_sorted) {
        free(sorted);
    }

    plan->type = type;
}

static inline void ecs_add_plan_build_added_only(
    ecs_world_t *world,
    ecs_table_t *from_table,
    ecs_component_t cid,
    const ecs_component_record_t *crec,
    ecs_add_plan_t *plan
) {
    ecs_add_plan_init(plan);
    ecs_add_plan_collect_requirements(world, from_table, plan, crec);
    ecs_add_plan_push(plan, cid);
}

void ecs_add_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);
    ecs_assert_can_be_updated(world, entity, "An abstract entity cannot be updated.");

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    uint16_t from_id = record->table_id;
    ecs_table_t *table = ecs_get_table(world, from_id);

    uint16_t edge = ecs_table_get_add_edge(table, cid);

    if (ECS_UNLIKELY(edge < table->type.count && table->type.ids[edge] == cid)) {
        return; // cid already present
    }

    const ecs_component_record_t *crec = ecs_component_index_get(&world->component_index, cid);
    ecs_add_plan_t plan = { 0 };
    ecs_add_plan_t *add_plan = NULL;

    if (edge == UINT16_MAX) {
        ecs_type_t new_type;
        if (crec->required_count) {
            ecs_add_plan_build_type(world, table, cid, crec, &plan);
            add_plan = &plan;
            new_type = plan.type;
        } else {
            new_type = ecs_type_with_add(&table->type, cid);
        }
        edge = ecs_table_index_get_or_create(world, new_type);

        // Re-fetch: ecs_table_index_get_or_create may realloc the tables vec
        table = ecs_get_table(world, from_id);
        ecs_id_map_set(&table->add_edge, cid, edge);
    } else if (ECS_UNLIKELY(edge < table->type.count && table->type.ids[edge] == cid)) {
        return;
    }

    ecs_table_t *new_table = ecs_get_table(world, edge);
    if (!add_plan && crec->required_count && new_table->type.count > table->type.count + 1) {
        ecs_add_plan_build_added_only(world, table, cid, crec, &plan);
        add_plan = &plan;
    }
    void *component_data =
        add_plan && add_plan->added_count > 1
            ? migrate_entity_add_many(world, record, entity, table, new_table, edge, cid)
            : migrate_entity_add(world, record, entity, table, new_table, edge, cid);

    if (add_plan) {
        for (uint16_t i = 0; i < add_plan->added_count; i++) {
            ecs_component_t added = add_plan->added[i];
            if (added == cid) {
                continue;
            }

            const ecs_component_record_t *added_rec =
                ecs_component_index_get(&world->component_index, added);
            void *added_data = ecs_table_component_at_column(
                new_table,
                ecs_table_get_column_index(new_table, added),
                record->table_row
            );
            if (added_rec->on_add) {
                added_rec->on_add(world, entity, added, added_data);
            }
            ecs_emit(world, new_table, entity, EcsOnAdd, added_data);
        }
    }
    if (crec->on_add) {
        crec->on_add(world, entity, cid, component_data);
        new_table = ecs_get_table(world, record->table_id);
    }
    ecs_emit(world, new_table, entity, EcsOnAdd, component_data);
    if (add_plan) {
        ecs_add_plan_fini(add_plan);
    }
}

void ecs_remove_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_entity_record_t *record = ecs_get_record(world, entity);
    uint16_t from_id = record->table_id;
    ecs_table_t *table = ecs_get_table(world, from_id);

    uint16_t col_idx = ecs_table_column_or_invalid(table, cid);

    if (ECS_UNLIKELY(col_idx == UINT16_MAX)) {
        return;
    }

    uint16_t new_table_id = table->cls[col_idx].remove_edge;
    if (new_table_id == UINT16_MAX) {
        ecs_type_t new_type = ecs_type_with_remove_at(&table->type, col_idx);
        new_table_id = ecs_table_index_get_or_create(world, new_type);
        // Re-fetch: ecs_table_index_get_or_create may realloc the tables vec
        table = ecs_get_table(world, from_id);
        table->cls[col_idx].remove_edge = new_table_id;
    }

    void *removed_data = ecs_table_component_at_column(table, col_idx, record->table_row);

    const ecs_component_record_t *crec = ecs_component_index_get(&world->component_index, cid);
    if (crec->on_remove) {
        crec->on_remove(world, entity, cid, removed_data);
        table = ecs_get_table(world, from_id);
    }
    ecs_emit(world, table, entity, EcsOnRemove, removed_data);

    migrate_entity_remove(world, record, entity, table, new_table_id, (uint16_t)col_idx);
}

void *ecs_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    const ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);
    return ecs_table_component_at_column(
        table,
        ecs_table_get_column_index(table, cid),
        record->table_row
    );
}

void *ecs_try_get_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    const ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);

    uint16_t col_idx = ecs_table_column_or_invalid(table, cid);
    if (col_idx != UINT16_MAX) {
        return ecs_table_component_at_column(table, col_idx, record->table_row);
    }
    return NULL;
}

void ecs_set_cid(ecs_world_t *world, ecs_entity_t entity, ecs_component_t cid, const void *data) {
    ecs_assert_not_null(world);
    ecs_assert_id_valid(cid);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    ecs_add_cid(world, entity, cid);
    const ecs_component_record_t *crec = ecs_component_index_get(&world->component_index, cid);
    ecs_entity_record_t *record = ecs_get_record(world, entity);
    ecs_table_t *table = ecs_get_table(world, record->table_id);
    uint16_t col_idx = ecs_table_get_column_index(table, cid);
    void *dst = ecs_table_component_at_column(table, col_idx, record->table_row);

    // on_set sees the new input data and the current table slot before copy.
    if (crec->on_set) {
        crec->on_set(world, entity, cid, data, dst);
        record = ecs_get_record(world, entity);
        table = ecs_get_table(world, record->table_id);
    }
    ecs_emit(world, table, entity, EcsOnSet, data);
    if (crec->size != 0) {
        memcpy(dst, data, crec->size);
    }
}

bool ecs_has_cid(const ecs_world_t *world, ecs_entity_t entity, ecs_component_t id) {
    ecs_assert_not_null(world);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    uint16_t tid = ecs_get_record(world, entity)->table_id;
    return ecs_table_has(world, ecs_get_table(world, tid), id);
}

bool ecs_has_cid_owned(const ecs_world_t *world, ecs_entity_t entity, ecs_component_t id) {
    ecs_assert_not_null(world);
    ecs_assert_entity_valid(entity);
    ecs_assert_is_alive(world, entity);

    uint16_t tid = ecs_get_record(world, entity)->table_id;
    return ecs_table_has_owned(ecs_get_table(world, tid), id);
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

    ecs_assert(record->tables.size == 0, "component already used cannot register requirement");

#ifndef NDEBUG
    for (uint32_t i = 0; i < record->required_count; i++) {
        if (record->required[i] == require) {
            ecs_assert(true, "required component already registered");
        }
    }
#endif

    record->required =
        realloc(record->required, sizeof(ecs_component_t) * (record->required_count + 1));
    record->required[record->required_count++] = require;
}

void ecs_fini(ecs_world_t *world) {
    ecs_resource_index_fini(&world->resource_index, world);
    ecs_entity_index_fini(&world->entity_index);
    ecs_component_index_fini(&world->component_index);
    ecs_table_index_fini(&world->table_index);
    ecs_query_index_fini(&world->query_index);
    ecs_observer_index_fini(&world->observer_index);
    ecs_system_index_fini(&world->system_index);
    ecs_module_index_fini(&world->module_index);
    ecs_arena_fini(&world->arena_allocator);
    sireflect_registry_fini(world->sireflect_registry);

    if (world->features.rest) {
        sihttp_server_stop(world->server);
    }
    sihttp_server_fini(world->server);

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
