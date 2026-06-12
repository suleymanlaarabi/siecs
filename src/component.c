#include "datastructure/string.h"
#include "datastructure/vec.h"
#include "sireflect.h"
#include "storage/component_index.h"
#include "utils.h"
#include "world_internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void RelationOnSet(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t target_component,
    const void *ptr
) {
    const RelationTarget *target_data = ptr;
    ecs_component_t source_component = target_component + 1;

    const RelationTarget *old_target_data = ecs_get_cid(world, entity, target_component);

    ecs_assert_entity_valid(target_data->target);
    ecs_assert_is_alive(world, target_data->target);

    if (old_target_data->target == target_data->target) {
        return;
    }

    if (old_target_data->target) {
        RelationSource *source = ecs_get_cid(world, old_target_data->target, source_component);

        ecs_vec_remove_u64(&source->entities, entity);
        if (source->entities.size == 0) {
            ecs_remove_cid(world, old_target_data->target, source_component);
        }
    }

    if (ecs_has_cid(world, target_data->target, source_component)) {
        RelationSource *source_data = ecs_get_cid(world, target_data->target, source_component);
        ecs_vec_push_u64(&source_data->entities, entity);
    } else {
        RelationSource source_data = {};
        ecs_vec_init(&source_data.entities, sizeof(ecs_entity_t));
        ecs_vec_push_u64(&source_data.entities, entity);
        ecs_set_cid(world, target_data->target, source_component, &source_data);
    }
}

void RelationOnRemove(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_component_t component,
    const void *ptr
) {
    const RelationTarget *target_data = ptr;
    ecs_component_t source_component = component + 1;
    RelationSource *target_source_data = ecs_get_cid(world, target_data->target, source_component);

    // Prevent recursive calls to RelationOnRemove when removing relation from child
    if (target_source_data->entities.size == UINT32_MAX) {
        return;
    }

    ecs_vec_remove_u64(&target_source_data->entities, entity);

    if (target_source_data->entities.size == 0) {
        ecs_remove_cid(world, target_data->target, source_component);
    }
}

void RelationSourceOnRemove(
    ecs_world_t *world,
    ecs_entity_t _entity,
    ecs_component_t component,
    const void *ptr
) {
    RelationSource *source_data = (void *)ptr;

    const ecs_entity_t *entities = source_data->entities.data;
    const uint32_t count = source_data->entities.size;

    // Prevent recursive calls to RelationOnRemove when removing relation from child
    source_data->entities.size = UINT32_MAX;
    for (uint32_t i = 0; i < count; i++) {
        ecs_remove_cid(world, entities[i], component - 1);
    }

    ecs_vec_fini(&source_data->entities);
}

ecs_component_t ecs_component_init(ecs_world_t *world, const ecs_component_desc_t *desc) {
    ecs_assert_not_null(world);
    ecs_assert_not_null(desc->name);

    sireflect_handle_t reflection = SIREFLECT_INVALID_HANDLE;

    if (desc->struct_desc) {
        reflection = sireflect_register_struct(world->sireflect_registry, desc->struct_desc);
    }

    if (desc->is_relation) {
        ecs_component_t component = ecs_component_index_create(
            &world->component_index,
            strdup(desc->name),
            desc->size,
            RelationOnSet,
            RelationOnRemove,
            reflection
        );

        ecs_str_t source_name = ecs_str_from_cstr("Source");
        ecs_str_cstr_append(&source_name, desc->name);

        ecs_component_index_create(
            &world->component_index,
            source_name.data,
            sizeof(RelationSource),
            NULL,
            RelationSourceOnRemove,
            SIREFLECT_INVALID_HANDLE
        );
        return component;
    } else {
        return ecs_component_index_create(
            &world->component_index,
            strdup(desc->name),
            desc->size,
            desc->on_set,
            desc->on_remove,
            reflection
        );
    }
}
