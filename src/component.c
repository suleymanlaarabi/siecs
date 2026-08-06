#include "helper.h"
#include "siecs.h"
#if SIECS_HAS_META && !defined(SIREFLECT_H)
#include "sireflect.h"
#endif
#if SIECS_HAS_META && !defined(SIJSON_H)
#include "sijson.h"
#endif
#include "relation.h"
#include "storage/component_index.h"
#include "utils.h"
#include "world_internal.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ecs_component_t ecs_component_alloc_ids(uint16_t count) {
    uint32_t id = ecs_world.component_index.components.size;
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

        sicore_vec_remove_u64(&source->entities, entity);
        if (source->entities.size == 0) {
            ecs_remove_cid(old_target_data->target, source_component);
        }
    }

    if (ecs_has_cid(target_data->target, source_component)) {
        RelationSource *source_data = ecs_get_cid(target_data->target, source_component);
        sicore_vec_push_u64(&source_data->entities, entity);
    } else {
        RelationSource source_data = {0};
        sicore_vec_init(&source_data.entities, sizeof(ecs_entity_t));
        sicore_vec_push_u64(&source_data.entities, entity);
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

    sicore_vec_remove_u64(&target_source_data->entities, entity);

    if (target_source_data->entities.size == 0) {
        ecs_remove_cid(target_data->target, source_component);
    }
}

static void RelationSourceDtor(void *ptr, uint32_t count) {
    RelationSource *source_data = ptr;
    for (uint32_t i = 0; i < count; i++) {
        sicore_vec_fini(&source_data[i].entities);
    }
}

void RelationSourceOnRemove(ecs_entity_t entity, ecs_component_t component, void *ptr) {
    (void)entity;
    RelationSource *source_data = ptr;

    const ecs_entity_t *entities = source_data->entities.data;
    const uint32_t count = source_data->entities.size;
    ecs_relation_id_t relation =
        ECS_COMPONENT_RELATION_ID(ecs_component_index_get(component)->relation_flags);
    const ecs_relation_record_t *relation_record = ecs_relation_record(relation);

    // Prevent recursive calls to RelationOnRemove when removing relation from child
    source_data->entities.size = UINT32_MAX;
    for (uint32_t i = 0; i < count; i++) {
        if (relation_record->on_delete_target == EcsDeleteSources) {
            ecs_kill(entities[i]);
        } else {
            ecs_unrelate_id(entities[i], relation);
        }
    }
}

static ecs_component_t ecs_component_register_type(
    ecs_component_t *id,
    const ecs_component_desc_t *desc
#if SIECS_HAS_META
    ,
    sireflect_handle_t type
#endif
) {
    ecs_assert_not_null(id);
    ecs_assert_not_null(desc);

    if (*id != 0) {
        const ecs_component_record_t *existing = ecs_component_index_get(*id);
        if (existing->tables.data) {
            return *id;
        }
    }

    if (*id == 0) {
        *id = ecs_component_alloc_ids(1);
    }

    ecs_component_t component = *id;
    ecs_component_index_register(
        component,
        desc->name,

        desc->size,
        desc->ops,
        desc->on_set,
        desc->on_remove,
        desc->on_add,
        0
#if SIECS_HAS_META
        ,
        type,
        desc->struct_desc
#endif
    );
    return component;
}

ecs_component_t ecs_component_register_relation_internal(
    const char *name,
    ecs_relation_id_t relation,
    bool by_target
) {
    ecs_component_t component = ecs_component_alloc_ids(by_target ? 1 : 2);
    uint32_t target_flags = ECS_COMPONENT_RELATION_FLAGS(relation, EcsComponentRelationTarget);
    ecs_component_index_register(
        component,
        name,

        by_target ? 0 : sizeof(RelationTarget),
        (ecs_type_ops_t){ 0 },
        by_target ? NULL : RelationOnSet,
        by_target ? ecs_relation_target_on_remove : RelationOnRemove,
        NULL,
        target_flags
#if SIECS_HAS_META
        ,
        SIREFLECT_INVALID_HANDLE,
        NULL
#endif
    );
    if (by_target) {
        return component;
    }
    ecs_component_index_register(
        component + 1,
        NULL,

        sizeof(RelationSource),
        (ecs_type_ops_t){ .dtor = RelationSourceDtor },
        NULL,
        RelationSourceOnRemove,
        NULL,
        ECS_COMPONENT_RELATION_FLAGS(relation, EcsComponentRelationSource)
#if SIECS_HAS_META
            ,
        SIREFLECT_INVALID_HANDLE,
        NULL
#endif
    );
    return component;
}

ecs_component_t ecs_component_register(ecs_component_t *id, const ecs_component_desc_t *desc) {
#if SIECS_HAS_META
    sireflect_handle_t type = SIREFLECT_INVALID_HANDLE;
    if (ECS_LIKELY(desc && desc->struct_desc)) {
        type = sireflect_try_register_struct(sijson_default_registry(), desc->struct_desc);
        if (ECS_UNLIKELY(type == SIREFLECT_INVALID_HANDLE)) {
            puts(sireflect_error());
        }
    }
    return ecs_component_register_type(id, desc, type);
#else
    return ecs_component_register_type(id, desc);
#endif
}

ecs_component_t ecs_component_init(const ecs_component_desc_t *desc) {
    ecs_component_t id = 0;
    return ecs_component_register(&id, desc);
}

const ecs_component_info_t *ecs_component_info(ecs_component_t component) {
    if (component == 0 || component >= ecs_world.component_index.components.size) {
        return NULL;
    }
    return ecs_component_index_get(component)->info;
}

uint32_t ecs_component_count(void) { return ecs_world.component_index.components.size; }

#if SIECS_HAS_META
ecs_component_t ecs_component_dynamic_init(const ecs_dynamic_component_desc_t *desc) {
    sireflect_registry_t *registry = sijson_default_registry();
    sireflect_handle_t type =
        sireflect_try_register_dynamic_struct(registry, desc->name, desc->fields);
    if (type == SIREFLECT_INVALID_HANDLE) {
        return 0;
    }

    for (uint32_t i = 1; i < ecs_world.component_index.components.size; i++) {
        const ecs_component_info_t *info = ecs_component_index_get((ecs_component_t)i)->info;
        if (info && info->type == type) {
            return (ecs_component_t)i;
        }
    }

    const sireflect_type_info_t *info = sireflect_type_info(registry, type);
    sireflect_struct_desc_t reflection = {
        .name = desc->name,
        .fields = desc->fields,
        .size = info->size,
        .align = info->align,
    };
    ecs_component_desc_t component = {
        .size = info->size,
        .struct_desc = &reflection,
    };

    component.name = desc->name;

    ecs_component_t id = 0;
    return ecs_component_register_type(&id, &component, type);
}

ecs_component_t ecs_tag_init(const char *name) {
    return ecs_component_dynamic_init(&(ecs_dynamic_component_desc_t){
        .name = name,
        .fields = "{}",
    });
}
#endif

const char *ecs_component_name(ecs_component_t component) {
    ecs_assert(
        component != 0 && component < ecs_world.component_index.components.size,
        "invalid component id: %u\n",
        component
    );
    return ecs_component_index_get(component)->info->name;
}
