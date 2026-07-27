#include "datastructure/vec.h"
#include "helper.h"
#include "siecs.h"
#include "sireflect.h"
#include "storage/component_index.h"
#include "utils.h"
#include "world_internal.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ecs_component_t ecs_component_alloc_ids(uint16_t count) {
    uint32_t id = ecs_world.component_index.components.size;
    if (id == 0)
        id = 1;
    ecs_assert(id + count <= UINT16_MAX, "component id overflow\n");
    return id;
}

void RelationOnSet(
    ecs_entity_t entity,
    ecs_component_t target_component,
    const void *new_value,
    void *current_value
) {
    const RelationTarget *target_data = new_value;
    ecs_component_t source_component = target_component + 1;

    const RelationTarget *old_target_data = current_value;

    ecs_assert_entity_valid(target_data->target);
    ecs_assert_is_alive(target_data->target);

    if (old_target_data->target == target_data->target) {
        return;
    }

    if (old_target_data->target) {
        RelationSource *source = ecs_get_cid(old_target_data->target, source_component);

        ecs_vec_remove_u64(&source->entities, entity);
        if (source->entities.size == 0) {
            ecs_remove_cid(old_target_data->target, source_component);
        }
    }

    if (ecs_has_cid(target_data->target, source_component)) {
        RelationSource *source_data = ecs_get_cid(target_data->target, source_component);
        ecs_vec_push_u64(&source_data->entities, entity);
    } else {
        RelationSource source_data = {};
        ecs_vec_init(&source_data.entities, sizeof(ecs_entity_t));
        ecs_vec_push_u64(&source_data.entities, entity);
        ecs_set_cid(target_data->target, source_component, &source_data);
    }
}

void RelationOnRemove(ecs_entity_t entity, ecs_component_t component, void *ptr) {
    const RelationTarget *target_data = ptr;
    ecs_component_t source_component = component + 1;
    RelationSource *target_source_data = ecs_get_cid(target_data->target, source_component);

    // Prevent recursive calls to RelationOnRemove when removing relation from child
    if (target_source_data->entities.size == UINT32_MAX) {
        return;
    }

    ecs_vec_remove_u64(&target_source_data->entities, entity);

    if (target_source_data->entities.size == 0) {
        ecs_remove_cid(target_data->target, source_component);
    }
}

void RelationSourceOnRemove(ecs_entity_t, ecs_component_t component, void *ptr) {
    RelationSource *source_data = ptr;

    const ecs_entity_t *entities = source_data->entities.data;
    const uint32_t count = source_data->entities.size;
    const ecs_component_record_t *crec = ecs_component_index_get(component);
    const bool cascade_delete = crec->relation_flags & EcsRelationCascadeDelete;

    // Prevent recursive calls to RelationOnRemove when removing relation from child
    source_data->entities.size = UINT32_MAX;
    for (uint32_t i = 0; i < count; i++) {
        if (cascade_delete) {
            ecs_kill(entities[i]);
        } else {
            ecs_remove_cid(entities[i], component - 1);
        }
    }

    ecs_vec_fini(&source_data->entities);
}

ecs_component_t ecs_component_register(ecs_component_t *id, const ecs_component_desc_t *desc) {
    ecs_assert_not_null(id);
    ecs_assert_not_null(desc);

    if (*id != 0 && *id < ecs_world.component_index.components.size) {
        const ecs_component_record_t *existing = ecs_component_index_get(*id);
        if (existing->tables.data) {
            return *id;
        }
    }

    sireflect_handle_t reflection = SIREFLECT_INVALID_HANDLE;

    if (ECS_LIKELY(desc->struct_desc)) {
        reflection = sireflect_try_register_struct(ecs_world.sireflect_registry, desc->struct_desc);

        if (ECS_UNLIKELY(reflection == SIREFLECT_INVALID_HANDLE)) {
            puts(sireflect_error());
        }
    }

    if (ECS_UNLIKELY(desc->relation_flags & EcsRelationTarget)) {
        if (*id == 0) {
            *id = ecs_component_alloc_ids(2);
        }

        ecs_component_t component = *id;
        ecs_component_index_register(
            component,
            desc->size,
            desc->ops,
            RelationOnSet,
            RelationOnRemove,
            desc->on_add,
            desc->relation_flags,
            reflection,
            desc->struct_desc
        );

        ecs_component_t source = component + 1;
        ecs_component_index_register(
            source,
            desc->relation_flags & EcsRelationOneToOne ? sizeof(RelationTarget)
                                                       : sizeof(RelationSource),
            (ecs_type_ops_t){ 0 },
            NULL,
            RelationSourceOnRemove,
            desc->on_add,
            (desc->relation_flags & ~EcsRelationTarget) | EcsRelationSource,
            SIREFLECT_INVALID_HANDLE,
            NULL
        );
        return component;
    } else {
        if (*id == 0) {
            *id = ecs_component_alloc_ids(1);
        }

        ecs_component_t component = *id;
        ecs_component_index_register(
            component,
            desc->size,
            desc->ops,
            desc->on_set,
            desc->on_remove,
            desc->on_add,
            0,
            reflection,
            desc->struct_desc
        );
        return component;
    }
}

ecs_component_t ecs_component_init(const ecs_component_desc_t *desc) {
    ecs_component_t id = 0;
    return ecs_component_register(&id, desc);
}
