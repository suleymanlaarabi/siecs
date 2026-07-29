#include "addons/addons.h"
#include "helper.h"
#include "sicore.h"
#include "siecs.h"
#include <stdio.h>
#if SIECS_HAS_META && !defined(SIREFLECT_H)
#include "sireflect.h"
#endif
#include "storage/table_index.h"
#include "world_internal.h"

ECS_RELATION_DEFINE(ChildOf, EcsRelationCascadeDelete);
#if SIECS_HAS_NAMES
sicore_map_t name_map;

void name_on_add(ecs_entity_t entity, ecs_component_t component, void *data) {
    Name *name = data;
    if (name->value) {
        sicore_map_set(&name_map, name->value, ecs_first(entity));
    }
}

void name_on_set(
    ecs_entity_t entity,
    ecs_component_t component,
    const void *new_value,
    void *current_value
) {
    Name *name = current_value;
    const Name *new_name = new_value;

    if (name->value) {
        sicore_map_unset(&name_map, name->value);
    }
    if (new_name->value) {
        sicore_map_set(&name_map, new_name->value, ecs_first(entity));
    }
}

void name_on_remove(ecs_entity_t entity, ecs_component_t component, void *data) {
    Name *name = data;
    if (name->value) {
        sicore_map_unset(&name_map, name->value);
    }
}

ECS_COMPONENT_DEFINE(
    Name,
    .on_add = name_on_add,
    .on_remove = name_on_remove,
    .on_set = name_on_set
);
#endif
ECS_TAG_DEFINE(Disabled);
ECS_TAG_DEFINE(Abstract);

void ecs_bootstrap() {
    // Reserve identifiers used to represent false return values.
    ecs_table_index_get_or_create((ecs_type_t){ 0 });
    sicore_vec_push_u64(&ecs_world.entity_index.entities, 0);
    ecs_component({ SIECS_NAME_INIT("Invalid") });

#if SIECS_HAS_META
    // Register the ecs_entity_t struct reflection.
    sireflect_register_struct(
        ecs_world.sireflect_registry,
        &(sireflect_struct_desc_t){
            .name = "ecs_entity_t",
            .fields = "{ uint32_t id; uint32_t generation; }",
            .size = sizeof(ecs_entity_t),
            .align = _Alignof(ecs_entity_t),
        }
    );
#endif

    ECS_COMPONENT_REGISTER(ChildOf);
#if SIECS_HAS_NAMES
    ECS_COMPONENT_REGISTER(Name);
    sicore_map_init(&name_map);
#endif
    ECS_COMPONENT_REGISTER(Disabled);
    ECS_COMPONENT_REGISTER(Abstract);

#if SIECS_HAS_REST
    if (ecs_world.features.rest) {
        init_rest();
    }
#endif
}
