#include "addons/addons.h"
#include "siecs.h"
#include "sireflect.h"
#include "storage/table_index.h"
#include "world_internal.h"

ECS_RELATION_DEFINE(ChildOf);
ECS_COMPONENT_DEFINE(IsA);
ECS_COMPONENT_DEFINE(Name);

void ecs_bootstrap(ecs_world_t *world) {
    // Reserve identifiers used to represent false return values.
    ecs_table_index_get_or_create(world, (ecs_type_t){ 0 });
    ecs_new(world);
    ecs_component(world, {});

    // Register the ecs_entity_t struct reflection.
    sireflect_register_struct(
        world->sireflect_registry,
        &(sireflect_struct_desc_t){
            .name = "ecs_entity_t",
            .fields = "{ uint32_t id; uint32_t generation; }",
            .size = sizeof(ecs_entity_t),
            .align = _Alignof(ecs_entity_t),
        }
    );

    ECS_COMPONENT_REGISTER(world, ChildOf);
    ECS_COMPONENT_REGISTER(world, IsA);
    ECS_COMPONENT_REGISTER(world, Name);

#ifdef SIECS_REST
    init_rest(world);
#endif
}
