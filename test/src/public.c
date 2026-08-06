#include "siecs.h"
#include <siecs_test.h>

ECS_COMPONENT_DECLARE(PublicPosition, {
    float x;
    float y;
});

ECS_COMPONENT_DEFINE(PublicPosition);

void public_metadata_and_entity_introspection(void) {
    ecs_init();
    ECS_COMPONENT_REGISTER(PublicPosition);

    const ecs_component_info_t *component = ecs_component_info(ecs_id(PublicPosition));
    test_not_null((void *)component);
    test_str("PublicPosition", component->name);
    test_not_null((void *)component->reflection);
    test_str("PublicPosition", component->reflection->name);
    test_true(ecs_component_count() > ecs_id(PublicPosition));

    const ecs_relation_info_t *child_of = ecs_relation_info(ecs_rid(ChildOf));
    test_not_null((void *)child_of);
    test_str("ChildOf", child_of->name);
    test_int(EcsRelationByDepth, child_of->desc.storage);
    test_true(child_of->desc.acyclic);
    test_true(ecs_relation_count() > ecs_rid(ChildOf));
    test_null((void *)ecs_relation_info(0));

    ecs_entity_t base = ecs_new();
    ecs_entity_t entity = ecs_new();
    ecs_is_a(entity, base);

    test_uint(entity, ecs_entity_from_index(ecs_entity_id(entity)));
    test_uint(base, ecs_entity_base(entity));
    test_uint(0, ecs_entity_base(base));

    ecs_kill(entity);
    test_uint(0, ecs_entity_from_index(ecs_entity_id(entity)));

    ecs_fini();
}
