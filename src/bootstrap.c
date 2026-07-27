#include "addons/addons.h"
#include "datastructure/vec.h"
#include "siecs.h"
#include "type.h"
#ifndef SIREFLECT_H
#include "sireflect.h"
#endif
#include "storage/table_index.h"
#include "world_internal.h"

ECS_RELATION_DEFINE(ChildOf, EcsRelationCascadeDelete);
ECS_COMPONENT_DEFINE(Name);
ECS_TAG_DEFINE(Disabled);
ECS_TAG_DEFINE(Abstract);

static ecs_component_t default_components[] = { 0 };

void ecs_bootstrap() {
    // Reserve identifiers used to represent false return values.
    ecs_vec_push_u64(&ecs_world.entity_index.entities, 0);
    ecs_component({ .name = "Invalid" });

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

    ECS_COMPONENT_REGISTER(ChildOf);
    ECS_COMPONENT_REGISTER(Name);
    ECS_COMPONENT_REGISTER(Disabled);
    ECS_COMPONENT_REGISTER(Abstract);

    default_components[0] = ecs_id(Name);
    ecs_table_index_get_or_create((ecs_type_t){ .ids = default_components, .count = 1 });
    init_rest();
}
