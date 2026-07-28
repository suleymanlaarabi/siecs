#include "addons/addons.h"
#include "datastructure/vec.h"
#include "siecs.h"
#if SIECS_HAS_META && !defined(SIREFLECT_H)
#include "sireflect.h"
#endif
#include "storage/table_index.h"
#include "world_internal.h"

ECS_RELATION_DEFINE(ChildOf, EcsRelationCascadeDelete);
#if SIECS_HAS_NAMES
ECS_COMPONENT_DEFINE(Name);
#endif
ECS_TAG_DEFINE(Disabled);
ECS_TAG_DEFINE(Abstract);

void ecs_bootstrap() {
    // Reserve identifiers used to represent false return values.
    ecs_table_index_get_or_create((ecs_type_t){ 0 });
    ecs_vec_push_u64(&ecs_world.entity_index.entities, 0);
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
#endif
    ECS_COMPONENT_REGISTER(Disabled);
    ECS_COMPONENT_REGISTER(Abstract);

#if SIECS_HAS_REST
    if (ecs_world.features.rest) {
        init_rest();
    }
#endif
}
